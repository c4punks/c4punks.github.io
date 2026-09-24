/**
 * @file main.c
 * @brief The C 4 Punk Developers organisation site, served by CWIST.
 *
 * The same binary does two jobs:
 *
 *   - `site.wasm`                serves the site over wasi:sockets;
 *   - `site.wasm --export DIR`   renders every route to static HTML in DIR,
 *                                which is what GitHub Pages publishes.
 *
 * Markup lives in the templates directory and is embedded into the binary at build
 * time by tools/embed.sh, so the WASI build needs no filesystem access.
 * Editorial content lives in content.h, projects.h and guides.h. Nothing on
 * the published site depends on JavaScript.
 */

#include <cwist/app.h>
#include <cwist/core/template/template.h>
#include <cwist/core/html/css_composer.h>
#include <cwist/core/db/sql.h>
#include <cwist/sys/app/assets.h>
#include <cjson/cJSON.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "content.h"
#include "projects.h"
#include "guides.h"
#include "templates.h"

#ifndef SITE_PORT
#define SITE_PORT 8080
#endif

static cwist_db *g_db = NULL;
static char g_css_url[256] = "/assets/css/app.css";

/* -------------------------------------------------------------------------- */
/* Small helpers                                                              */
/* -------------------------------------------------------------------------- */

/** @brief Append a printf-formatted fragment to an sstring. */
static void s_appendf(cwist_sstring *s, const char *fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    cwist_sstring_append(s, buf);
}

/** @brief Double every single quote so a value can sit inside an SQL literal. */
static void sql_quote(const char *src, char *dst, size_t cap) {
    size_t o = 0;
    for (const char *p = src; *p && o + 2 < cap; p++) {
        if (*p == '\'') dst[o++] = '\'';
        dst[o++] = *p;
    }
    dst[o < cap ? o : cap - 1] = '\0';
}

/** @brief Copy text with HTML tags and entities removed, for the search index. */
static void strip_markup(const char *src, char *dst, size_t cap) {
    size_t o = 0;
    bool in_tag = false;
    for (const char *p = src; *p && o + 1 < cap; p++) {
        if (*p == '<') { in_tag = true; continue; }
        if (*p == '>') { in_tag = false; dst[o++] = ' '; continue; }
        if (in_tag) continue;
        if (*p == '&') {
            while (*p && *p != ';' && *p != ' ') p++;
            if (!*p) break;
            if (*p == ' ') dst[o++] = ' ';
            continue;
        }
        dst[o++] = *p;
    }
    dst[o] = '\0';
}

/** @brief Escape the five characters that matter inside HTML text or attributes. */
static void html_escape(const char *src, char *dst, size_t cap) {
    size_t o = 0;
    for (const char *p = src; *p && o + 7 < cap; p++) {
        switch (*p) {
            case '&':  memcpy(dst + o, "&amp;", 5);  o += 5; break;
            case '<':  memcpy(dst + o, "&lt;", 4);   o += 4; break;
            case '>':  memcpy(dst + o, "&gt;", 4);   o += 4; break;
            case '"':  memcpy(dst + o, "&quot;", 6); o += 6; break;
            case '\'': memcpy(dst + o, "&#39;", 5);  o += 5; break;
            default:   dst[o++] = *p; break;
        }
    }
    dst[o] = '\0';
}

/* -------------------------------------------------------------------------- */
/* Shared template context                                                    */
/* -------------------------------------------------------------------------- */

/** @brief Build the site-wide context: branding, navigation and asset URLs. */
static cJSON *base_context(const char *active) {
    static const struct { const char *href; const char *label; } nav[] = {
        { "/projects/",   "Projects"   },
        { "/guides/",     "Guides"     },
        { "/about/",      "About"      },
        { "/contribute/", "Contribute" },
        { "/search/",     "Search"     }
    };

    cJSON *ctx = cJSON_CreateObject();

    cJSON *site = cJSON_CreateObject();
    cJSON_AddStringToObject(site, "org", SITE_ORG);
    cJSON_AddStringToObject(site, "base", SITE_BASE);
    cJSON_AddStringToObject(site, "github", SITE_GITHUB);
    cJSON_AddStringToObject(site, "discord", SITE_DISCORD);
    cJSON_AddStringToObject(site, "year", SITE_YEAR);
    cJSON_AddStringToObject(site, "blurb", SITE_BLURB);
    cJSON_AddItemToObject(ctx, "site", site);

    cJSON *items = cJSON_CreateArray();
    for (size_t i = 0; i < sizeof(nav) / sizeof(nav[0]); i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "href", nav[i].href);
        cJSON_AddStringToObject(item, "label", nav[i].label);
        cJSON_AddStringToObject(item, "cls",
            (active && strcmp(active, nav[i].href) == 0)
                ? "nav-link is-active" : "nav-link");
        cJSON_AddItemToArray(items, item);
    }
    cJSON_AddItemToObject(ctx, "nav", items);

    cJSON_AddStringToObject(ctx, "css", g_css_url);
    return ctx;
}

/** @brief Wrap rendered page content in the site layout. */
static cwist_sstring *render_layout(const char *title, const char *description,
                                    const char *canonical, const char *active,
                                    const char *content_html) {
    cJSON *ctx = base_context(active);
    cJSON_AddStringToObject(ctx, "title", title);
    cJSON_AddStringToObject(ctx, "description", description);
    cJSON_AddStringToObject(ctx, "canonical", canonical);
    cJSON_AddStringToObject(ctx, "content", content_html);

    cwist_sstring *out = cwist_template_render(TPL_LAYOUT_HTML, ctx);
    cJSON_Delete(ctx);
    return out;
}

/** @brief Render the standard page banner used by every non-home page. */
static cwist_sstring *render_pagehead(const char *kicker, const char *heading,
                                      const char *subheading, const char *links_html) {
    cJSON *ctx = cJSON_CreateObject();
    cJSON_AddStringToObject(ctx, "kicker", kicker);
    cJSON_AddStringToObject(ctx, "heading", heading);
    cJSON_AddStringToObject(ctx, "subheading", subheading);
    cJSON_AddStringToObject(ctx, "head_links", links_html ? links_html : "");
    cwist_sstring *out = cwist_template_render(TPL_PAGE_HEAD_HTML, ctx);
    cJSON_Delete(ctx);
    return out;
}

/** @brief Serialise the project list into a template array. */
static cJSON *projects_json(void) {
    cJSON *arr = cJSON_CreateArray();
    for (size_t i = 0; i < g_project_count; i++) {
        const project_t *p = &g_projects[i];
        char href[128];
        snprintf(href, sizeof(href), "/projects/%s/", p->slug);

        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", p->name);
        cJSON_AddStringToObject(o, "tag", p->tag);
        cJSON_AddStringToObject(o, "tagline", p->tagline);
        cJSON_AddStringToObject(o, "summary", p->summary);
        cJSON_AddStringToObject(o, "bullets_html", p->bullets_html);
        cJSON_AddStringToObject(o, "href", href);
        cJSON_AddStringToObject(o, "repo", p->repo);
        cJSON_AddItemToArray(arr, o);
    }
    return arr;
}

/** @brief Serialise guides into a template array, optionally filtered by project. */
static cJSON *guides_json(const char *project_filter, const char *skip_slug) {
    cJSON *arr = cJSON_CreateArray();
    for (size_t i = 0; i < g_guide_count; i++) {
        const guide_t *g = &g_guides[i];
        if (project_filter && strcmp(project_filter, g->project) != 0) continue;
        if (skip_slug && strcmp(skip_slug, g->slug) == 0) continue;

        char href[128];
        snprintf(href, sizeof(href), "/guides/%s/", g->slug);

        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "title", g->title);
        cJSON_AddStringToObject(o, "summary", g->summary);
        cJSON_AddStringToObject(o, "project", g->project);
        cJSON_AddStringToObject(o, "level", g->level);
        cJSON_AddStringToObject(o, "minutes", g->minutes);
        cJSON_AddStringToObject(o, "href", href);
        cJSON_AddItemToArray(arr, o);
    }
    return arr;
}

/** @brief Serialise a feature list into a template array. */
static cJSON *features_json(const feature_t *features, size_t count) {
    cJSON *arr = cJSON_CreateArray();
    for (size_t i = 0; i < count; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "mark", features[i].mark);
        cJSON_AddStringToObject(o, "title", features[i].title);
        cJSON_AddStringToObject(o, "body", features[i].body);
        cJSON_AddItemToArray(arr, o);
    }
    return arr;
}

/* -------------------------------------------------------------------------- */
/* Pages                                                                      */
/* -------------------------------------------------------------------------- */

static cwist_sstring *page_home(void) {
    cJSON *ctx = base_context("/");

    cJSON *stats = cJSON_CreateArray();
    for (size_t i = 0; i < sizeof(g_stats) / sizeof(g_stats[0]); i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "n", g_stats[i].n);
        cJSON_AddStringToObject(o, "l", g_stats[i].l);
        cJSON_AddItemToArray(stats, o);
    }
    cJSON_AddItemToObject(ctx, "stats", stats);
    cJSON_AddItemToObject(ctx, "projects", projects_json());
    cJSON_AddItemToObject(ctx, "principles",
        features_json(g_principles, sizeof(g_principles) / sizeof(g_principles[0])));
    cJSON_AddItemToObject(ctx, "guides", guides_json(NULL, NULL));
    cJSON_AddStringToObject(ctx, "hello_code", g_cwist_code);

    cwist_sstring *body = cwist_template_render(TPL_HOME_HTML, ctx);
    cJSON_Delete(ctx);
    if (!body) return NULL;

    cwist_sstring *page = render_layout(
        SITE_ORG " — open-source systems software in C",
        "C 4 Punk Developers is an independent open-source group building "
        "systems software in C: CWIST, a web framework and application server, "
        "and libttak, a deterministic systems runtime.",
        "/", "/", body->data);
    cwist_sstring_destroy(body);
    return page;
}

static cwist_sstring *page_projects(void) {
    cJSON *ctx = base_context("/projects/");
    cJSON_AddItemToObject(ctx, "projects", projects_json());

    cJSON *sats = cJSON_CreateArray();
    for (size_t i = 0; i < sizeof(g_satellites) / sizeof(g_satellites[0]); i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", g_satellites[i].name);
        cJSON_AddStringToObject(o, "body", g_satellites[i].body);
        cJSON_AddStringToObject(o, "repo", g_satellites[i].repo);
        cJSON_AddItemToArray(sats, o);
    }
    cJSON_AddItemToObject(ctx, "satellites", sats);

    cwist_sstring *body = cwist_template_render(TPL_PROJECTS_HTML, ctx);
    cJSON_Delete(ctx);
    if (!body) return NULL;

    cwist_sstring *head = render_pagehead(
        "Projects", "What we maintain",
        "Two flagship repositories and the supporting work around them. "
        "Everything here is public and permissively licensed.", NULL);

    cwist_sstring *content = cwist_sstring_create();
    cwist_sstring_append(content, head ? head->data : "");
    cwist_sstring_append(content, body->data);

    cwist_sstring *page = render_layout(
        "Projects — " SITE_ORG,
        "The open-source projects maintained by C 4 Punk Developers: CWIST, "
        "libttak, and the supporting repositories around them.",
        "/projects/", "/projects/", content->data);

    cwist_sstring_destroy(content);
    if (head) cwist_sstring_destroy(head);
    cwist_sstring_destroy(body);
    return page;
}

static cwist_sstring *page_project(const project_t *p) {
    char canonical[128], links[512], title[256], desc[512];
    snprintf(canonical, sizeof(canonical), "/projects/%s/", p->slug);
    snprintf(links, sizeof(links),
        "<div class=\"links\"><a href=\"%s\" rel=\"noopener\">Repository &rarr;</a>"
        "<a href=\"/guides/\">Guides &rarr;</a>"
        "<a href=\"/contribute/\">Contribute &rarr;</a></div>", p->repo);
    snprintf(title, sizeof(title), "%s — %s", p->name, SITE_ORG);
    snprintf(desc, sizeof(desc), "%s. %s", p->tagline, p->summary);

    cJSON *ctx = base_context("/projects/");
    cJSON_AddStringToObject(ctx, "name", p->name);
    cJSON_AddStringToObject(ctx, "body_html", p->body_html);
    cJSON_AddStringToObject(ctx, "code", p->code);
    cJSON_AddStringToObject(ctx, "code_label", p->code_label);
    cJSON_AddStringToObject(ctx, "glance", p->glance);
    cJSON_AddStringToObject(ctx, "repo", p->repo);
    cJSON_AddItemToObject(ctx, "features", features_json(p->features, p->feature_count));
    cJSON_AddItemToObject(ctx, "guides", guides_json(p->name, NULL));

    cwist_sstring *body = cwist_template_render(TPL_PROJECT_HTML, ctx);
    cJSON_Delete(ctx);
    if (!body) return NULL;

    cwist_sstring *head = render_pagehead("Project", p->name, p->tagline, links);

    cwist_sstring *content = cwist_sstring_create();
    cwist_sstring_append(content, head ? head->data : "");
    cwist_sstring_append(content, body->data);

    cwist_sstring *page = render_layout(title, desc, canonical, "/projects/",
                                        content->data);
    cwist_sstring_destroy(content);
    if (head) cwist_sstring_destroy(head);
    cwist_sstring_destroy(body);
    return page;
}

static cwist_sstring *page_guides(void) {
    cJSON *ctx = base_context("/guides/");
    cJSON_AddItemToObject(ctx, "guides", guides_json(NULL, NULL));

    cwist_sstring *body = cwist_template_render(TPL_GUIDES_HTML, ctx);
    cJSON_Delete(ctx);
    if (!body) return NULL;

    cwist_sstring *head = render_pagehead(
        "Guides", "Learn our projects properly",
        "Step-by-step walkthroughs with every command written out and the "
        "reasoning behind each step included.", NULL);

    cwist_sstring *content = cwist_sstring_create();
    cwist_sstring_append(content, head ? head->data : "");
    cwist_sstring_append(content, body->data);

    cwist_sstring *page = render_layout(
        "Guides — " SITE_ORG,
        "Step-by-step guides to CWIST and libttak from C 4 Punk Developers.",
        "/guides/", "/guides/", content->data);

    cwist_sstring_destroy(content);
    if (head) cwist_sstring_destroy(head);
    cwist_sstring_destroy(body);
    return page;
}

static cwist_sstring *page_guide(const guide_t *g) {
    char canonical[128], title[256], sub[512];
    snprintf(canonical, sizeof(canonical), "/guides/%s/", g->slug);
    snprintf(title, sizeof(title), "%s — " SITE_ORG, g->title);
    snprintf(sub, sizeof(sub), "%s", g->summary);

    char kicker[128];
    snprintf(kicker, sizeof(kicker), "%s guide · %s · %s min read",
             g->project, g->level, g->minutes);

    cJSON *ctx = base_context("/guides/");
    cJSON_AddStringToObject(ctx, "body_html", g->body_html);
    cJSON_AddItemToObject(ctx, "guides", guides_json(NULL, g->slug));

    cwist_sstring *body = cwist_template_render(TPL_GUIDE_HTML, ctx);
    cJSON_Delete(ctx);
    if (!body) return NULL;

    cwist_sstring *head = render_pagehead(kicker, g->title, sub, NULL);

    cwist_sstring *content = cwist_sstring_create();
    cwist_sstring_append(content, head ? head->data : "");
    cwist_sstring_append(content, body->data);

    cwist_sstring *page = render_layout(title, g->summary, canonical, "/guides/",
                                        content->data);
    cwist_sstring_destroy(content);
    if (head) cwist_sstring_destroy(head);
    cwist_sstring_destroy(body);
    return page;
}

/** @brief Render a prose page (about, contribute) from a stored HTML body. */
static cwist_sstring *page_prose(const char *kicker, const char *heading,
                                 const char *sub, const char *canonical,
                                 const char *title, const char *desc,
                                 const char *body_html) {
    cJSON *ctx = base_context(canonical);
    cJSON_AddStringToObject(ctx, "body_html", body_html);

    cwist_sstring *body = cwist_template_render(TPL_PROSE_HTML, ctx);
    cJSON_Delete(ctx);
    if (!body) return NULL;

    cwist_sstring *head = render_pagehead(kicker, heading, sub, NULL);

    cwist_sstring *content = cwist_sstring_create();
    cwist_sstring_append(content, head ? head->data : "");
    cwist_sstring_append(content, body->data);

    cwist_sstring *page = render_layout(title, desc, canonical, canonical,
                                        content->data);
    cwist_sstring_destroy(content);
    if (head) cwist_sstring_destroy(head);
    cwist_sstring_destroy(body);
    return page;
}

/* -------------------------------------------------------------------------- */
/* Search                                                                     */
/* -------------------------------------------------------------------------- */

/** @brief Build the in-memory index of every page the site publishes. */
static int build_index(void) {
    if (cwist_db_exec(g_db,
            "CREATE TABLE IF NOT EXISTS pages ("
            "url TEXT PRIMARY KEY, title TEXT, kind TEXT, body TEXT)")
            .error.err_i16 != 0) {
        return -1;
    }

    struct { const char *url, *title, *kind, *body; } fixed[] = {
        { "/", SITE_ORG, "Organisation",
          "Open-source systems software in C. CWIST web framework application "
          "server and libttak deterministic runtime. Plain C, vendored "
          "dependencies, honest benchmarks, public by default." },
        { "/about/", "About " SITE_ORG, "Organisation", NULL },
        { "/contribute/", "Contribute", "Organisation", NULL },
        { "/projects/", "Projects", "Index",
          "All repositories maintained by the organisation, including "
          "libttak-books, the Homebrew tap and this site." }
    };

    char stripped_about[16384];
    char stripped_contribute[16384];
    strip_markup(g_about_html, stripped_about, sizeof(stripped_about));
    strip_markup(g_contribute_html, stripped_contribute, sizeof(stripped_contribute));
    fixed[1].body = stripped_about;
    fixed[2].body = stripped_contribute;

    char sql[65536];
    char q_title[512], q_body[32768];

    for (size_t i = 0; i < sizeof(fixed) / sizeof(fixed[0]); i++) {
        sql_quote(fixed[i].title, q_title, sizeof(q_title));
        sql_quote(fixed[i].body, q_body, sizeof(q_body));
        snprintf(sql, sizeof(sql),
            "INSERT OR REPLACE INTO pages VALUES ('%s','%s','%s','%s')",
            fixed[i].url, q_title, fixed[i].kind, q_body);
        cwist_db_exec(g_db, sql);
    }

    for (size_t i = 0; i < g_project_count; i++) {
        const project_t *p = &g_projects[i];
        char body[32768], stripped[32768];
        strip_markup(p->body_html, stripped, sizeof(stripped));
        snprintf(body, sizeof(body), "%s %s %s %s",
                 p->tagline, p->summary, p->search_text, stripped);

        sql_quote(p->name, q_title, sizeof(q_title));
        sql_quote(body, q_body, sizeof(q_body));
        snprintf(sql, sizeof(sql),
            "INSERT OR REPLACE INTO pages VALUES ('/projects/%s/','%s','Project','%s')",
            p->slug, q_title, q_body);
        cwist_db_exec(g_db, sql);
    }

    for (size_t i = 0; i < g_guide_count; i++) {
        const guide_t *g = &g_guides[i];
        char body[32768], stripped[32768];
        strip_markup(g->body_html, stripped, sizeof(stripped));
        snprintf(body, sizeof(body), "%s %s %s", g->summary, g->project, stripped);

        sql_quote(g->title, q_title, sizeof(q_title));
        sql_quote(body, q_body, sizeof(q_body));
        snprintf(sql, sizeof(sql),
            "INSERT OR REPLACE INTO pages VALUES ('/guides/%s/','%s','Guide','%s')",
            g->slug, q_title, q_body);
        cwist_db_exec(g_db, sql);
    }
    return 0;
}

/** @brief Render the result rows for a query, or the full page directory when empty. */
static cwist_sstring *search_results(const char *q) {
    cwist_sstring *out = cwist_sstring_create();

    const char *sql_all = "SELECT url, title, kind, body FROM pages ORDER BY kind, title";
    char sql[4096];
    const char *stmt = sql_all;

    if (q && *q) {
        char esc[512];
        sql_quote(q, esc, sizeof(esc));
        snprintf(sql, sizeof(sql),
            "SELECT url, title, kind, body FROM pages "
            "WHERE title LIKE '%%%s%%' OR body LIKE '%%%s%%' "
            "ORDER BY kind, title LIMIT 25", esc, esc);
        stmt = sql;
    } else {
        cwist_sstring_append(out,
            "<p style=\"color:var(--muted);margin:0 0 20px\">Everything this site "
            "publishes, in one list. Type above to filter it.</p>");
    }

    cJSON *rows = NULL;
    cwist_error_t err = cwist_db_query(g_db, stmt, &rows);
    if (err.error.err_i16 != 0 || !rows) {
        cwist_sstring_append(out, "<p>Search is unavailable right now.</p>");
        if (rows) cJSON_Delete(rows);
        return out;
    }

    int n = cJSON_GetArraySize(rows);
    if (n == 0) {
        char esc[512];
        html_escape(q ? q : "", esc, sizeof(esc));
        s_appendf(out, "<p>No page matches <strong>%s</strong>. "
                       "Try a project name, or browse the "
                       "<a href=\"/guides/\">guides</a>.</p>", esc);
        cJSON_Delete(rows);
        return out;
    }

    if (q && *q) {
        char esc[512];
        html_escape(q, esc, sizeof(esc));
        s_appendf(out, "<p style=\"color:var(--muted);margin:0 0 20px\">"
                       "%d result%s for <strong>%s</strong>.</p>",
                  n, n == 1 ? "" : "s", esc);
    }

    cwist_sstring_append(out, "<div class=\"rows\">");
    for (int i = 0; i < n; i++) {
        cJSON *row = cJSON_GetArrayItem(rows, i);
        const char *url = cJSON_GetStringValue(cJSON_GetObjectItem(row, "url"));
        const char *title = cJSON_GetStringValue(cJSON_GetObjectItem(row, "title"));
        const char *kind = cJSON_GetStringValue(cJSON_GetObjectItem(row, "kind"));
        const char *body = cJSON_GetStringValue(cJSON_GetObjectItem(row, "body"));

        char snippet[260] = {0};
        if (body) {
            snprintf(snippet, sizeof(snippet), "%.230s", body);
            if (strlen(body) > 230) strcat(snippet, "…");
        }
        char e_title[512], e_snippet[1024];
        html_escape(title ? title : "", e_title, sizeof(e_title));
        html_escape(snippet, e_snippet, sizeof(e_snippet));

        s_appendf(out,
            "<a class=\"row\" href=\"%s\"><h3>%s</h3><p>%s</p>"
            "<span class=\"meta\">%s &middot; %s</span></a>",
            url ? url : "/", e_title, e_snippet, kind ? kind : "Page",
            url ? url : "/");
    }
    cwist_sstring_append(out, "</div>");

    cJSON_Delete(rows);
    return out;
}

static cwist_sstring *page_search(const char *q, bool live) {
    char e_q[512];
    html_escape(q ? q : "", e_q, sizeof(e_q));

    cwist_sstring *results = search_results(q);

    cJSON *ctx = base_context("/search/");
    cJSON_AddStringToObject(ctx, "q", e_q);
    cJSON_AddStringToObject(ctx, "results_html", results->data);

    cwist_sstring *body = cwist_template_render(TPL_SEARCH_HTML, ctx);
    cJSON_Delete(ctx);
    cwist_sstring_destroy(results);
    if (!body) return NULL;

    cwist_sstring *head = render_pagehead("Search", "Find a page",
        live ? "Full-text search across every project page, guide and "
               "organisation page on this site."
             : "This is the static export, so the index below is the whole site. "
               "Live full-text search runs when the CWIST server serves this site "
               "directly.", NULL);

    cwist_sstring *content = cwist_sstring_create();
    cwist_sstring_append(content, head ? head->data : "");
    cwist_sstring_append(content, body->data);

    cwist_sstring *page = render_layout("Search — " SITE_ORG,
        "Search the C 4 Punk Developers site.", "/search/", "/search/",
        content->data);

    cwist_sstring_destroy(content);
    if (head) cwist_sstring_destroy(head);
    cwist_sstring_destroy(body);
    return page;
}

/* -------------------------------------------------------------------------- */
/* Sitemap and robots                                                         */
/* -------------------------------------------------------------------------- */

static cwist_sstring *build_sitemap(void) {
    cwist_sstring *out = cwist_sstring_create();
    cwist_sstring_append(out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n");

    static const char *fixed[] = { "/", "/projects/", "/guides/", "/about/",
                                   "/contribute/", "/search/" };
    for (size_t i = 0; i < sizeof(fixed) / sizeof(fixed[0]); i++) {
        s_appendf(out, "  <url><loc>%s%s</loc></url>\n", SITE_BASE, fixed[i]);
    }
    for (size_t i = 0; i < g_project_count; i++) {
        s_appendf(out, "  <url><loc>%s/projects/%s/</loc></url>\n",
                  SITE_BASE, g_projects[i].slug);
    }
    for (size_t i = 0; i < g_guide_count; i++) {
        s_appendf(out, "  <url><loc>%s/guides/%s/</loc></url>\n",
                  SITE_BASE, g_guides[i].slug);
    }
    cwist_sstring_append(out, "</urlset>\n");
    return out;
}

static const char g_robots[] =
    "User-agent: *\n"
    "Allow: /\n"
    "Sitemap: " SITE_BASE "/sitemap.xml\n";

/* -------------------------------------------------------------------------- */
/* Stylesheet                                                                 */
/* -------------------------------------------------------------------------- */

/** @brief Minify the stylesheet, register it as an asset, and cache its URL. */
static void register_styles(cwist_app *app) {
    const char *parts[] = { TPL_APP_CSS };
    cwist_sstring *bundle = cwist_css_bundle(parts, 1, true);
    const char *css = bundle && bundle->data ? bundle->data : TPL_APP_CSS;

    cwist_app_asset_add(app, "css/app.css", css, strlen(css),
                        "text/css; charset=utf-8");

    const char *url = cwist_app_asset_url(app, "css/app.css");
    if (url) snprintf(g_css_url, sizeof(g_css_url), "%s", url);

    if (bundle) cwist_sstring_destroy(bundle);
}

/* -------------------------------------------------------------------------- */
/* HTTP handlers                                                              */
/* -------------------------------------------------------------------------- */

static void send_page(cwist_http_response *res, cwist_sstring *page, int status) {
    res->status_code = status;
    cwist_http_response_add_security_headers(res);
    cwist_http_header_add(&res->headers, "Content-Type", "text/html; charset=utf-8");
    cwist_sstring_assign(res->body, page && page->data ? page->data : "");
    if (page) cwist_sstring_destroy(page);
}

static void h_home(cwist_http_request *req, cwist_http_response *res) {
    (void)req;
    send_page(res, page_home(), CWIST_HTTP_OK);
}

static void h_projects(cwist_http_request *req, cwist_http_response *res) {
    (void)req;
    send_page(res, page_projects(), CWIST_HTTP_OK);
}

static void h_project(cwist_http_request *req, cwist_http_response *res) {
    const char *slug = cwist_query_map_get(req->path_params, "slug");
    for (size_t i = 0; slug && i < g_project_count; i++) {
        if (strcmp(slug, g_projects[i].slug) == 0) {
            send_page(res, page_project(&g_projects[i]), CWIST_HTTP_OK);
            return;
        }
    }
    cwist_sstring *body = cwist_template_render(TPL_NOTFOUND_HTML, NULL);
    cwist_sstring *head = render_pagehead("404", "No such project",
        "That project is not one of ours, or the link has moved.", NULL);
    cwist_sstring *content = cwist_sstring_create();
    cwist_sstring_append(content, head ? head->data : "");
    cwist_sstring_append(content, body ? body->data : "");
    send_page(res, render_layout("Not found — " SITE_ORG, "Page not found.",
                                 "/projects/", "/projects/", content->data),
              CWIST_HTTP_NOT_FOUND);
    cwist_sstring_destroy(content);
    if (head) cwist_sstring_destroy(head);
    if (body) cwist_sstring_destroy(body);
}

static void h_guides(cwist_http_request *req, cwist_http_response *res) {
    (void)req;
    send_page(res, page_guides(), CWIST_HTTP_OK);
}

static void h_guide(cwist_http_request *req, cwist_http_response *res) {
    const char *slug = cwist_query_map_get(req->path_params, "slug");
    for (size_t i = 0; slug && i < g_guide_count; i++) {
        if (strcmp(slug, g_guides[i].slug) == 0) {
            send_page(res, page_guide(&g_guides[i]), CWIST_HTTP_OK);
            return;
        }
    }
    cwist_sstring *body = cwist_template_render(TPL_NOTFOUND_HTML, NULL);
    cwist_sstring *head = render_pagehead("404", "No such guide",
        "That guide does not exist. The index lists every one we publish.", NULL);
    cwist_sstring *content = cwist_sstring_create();
    cwist_sstring_append(content, head ? head->data : "");
    cwist_sstring_append(content, body ? body->data : "");
    send_page(res, render_layout("Not found — " SITE_ORG, "Page not found.",
                                 "/guides/", "/guides/", content->data),
              CWIST_HTTP_NOT_FOUND);
    cwist_sstring_destroy(content);
    if (head) cwist_sstring_destroy(head);
    if (body) cwist_sstring_destroy(body);
}

static void h_about(cwist_http_request *req, cwist_http_response *res) {
    (void)req;
    send_page(res, page_prose("About", "An independent group writing C",
        "Who we are, why we still write C, and how decisions get made.",
        "/about/", "About — " SITE_ORG,
        "About C 4 Punk Developers: an independent open-source group building "
        "systems software in C.", g_about_html), CWIST_HTTP_OK);
}

static void h_contribute(cwist_http_request *req, cwist_http_response *res) {
    (void)req;
    send_page(res, page_prose("Contribute", "Send the patch",
        "What a good contribution looks like, how review works, and what we "
        "expect from each other.",
        "/contribute/", "Contribute — " SITE_ORG,
        "How to contribute to the open-source projects maintained by "
        "C 4 Punk Developers.", g_contribute_html), CWIST_HTTP_OK);
}

static void h_search(cwist_http_request *req, cwist_http_response *res) {
    const char *q = cwist_query_map_get(req->query_params, "q");
    send_page(res, page_search(q, true), CWIST_HTTP_OK);
}

static void h_sitemap(cwist_http_request *req, cwist_http_response *res) {
    (void)req;
    cwist_sstring *xml = build_sitemap();
    res->status_code = CWIST_HTTP_OK;
    cwist_http_header_add(&res->headers, "Content-Type", "application/xml; charset=utf-8");
    cwist_sstring_assign(res->body, xml->data);
    cwist_sstring_destroy(xml);
}

static void h_robots(cwist_http_request *req, cwist_http_response *res) {
    (void)req;
    res->status_code = CWIST_HTTP_OK;
    cwist_http_header_add(&res->headers, "Content-Type", "text/plain; charset=utf-8");
    cwist_sstring_assign(res->body, g_robots);
}

/* -------------------------------------------------------------------------- */
/* Static export                                                              */
/* -------------------------------------------------------------------------- */

static int write_file(const char *root, const char *rel, const char *data, size_t len) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", root, rel);

    for (char *p = path + strlen(root) + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        mkdir(path, 0755);
        *p = '/';
    }

    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "site: cannot write %s\n", path);
        return -1;
    }
    fwrite(data, 1, len, f);
    fclose(f);
    printf("  %s (%zu bytes)\n", rel, len);
    return 0;
}

static int export_page(const char *root, const char *rel, cwist_sstring *page) {
    if (!page) return -1;
    int rc = write_file(root, rel, page->data, page->size ? page->size
                                                          : strlen(page->data));
    cwist_sstring_destroy(page);
    return rc;
}

/** @brief Render every route to static HTML under @p root for GitHub Pages. */
static int export_site(cwist_app *app, const char *root) {
    printf("site: exporting to %s\n", root);
    mkdir(root, 0755);

    int rc = 0;
    rc |= export_page(root, "index.html", page_home());
    rc |= export_page(root, "projects/index.html", page_projects());
    rc |= export_page(root, "guides/index.html", page_guides());
    rc |= export_page(root, "search/index.html", page_search(NULL, false));
    rc |= export_page(root, "404.html", page_search(NULL, false));

    rc |= export_page(root, "about/index.html",
        page_prose("About", "An independent group writing C",
            "Who we are, why we still write C, and how decisions get made.",
            "/about/", "About — " SITE_ORG,
            "About C 4 Punk Developers: an independent open-source group "
            "building systems software in C.", g_about_html));

    rc |= export_page(root, "contribute/index.html",
        page_prose("Contribute", "Send the patch",
            "What a good contribution looks like, how review works, and what we "
            "expect from each other.",
            "/contribute/", "Contribute — " SITE_ORG,
            "How to contribute to the open-source projects maintained by "
            "C 4 Punk Developers.", g_contribute_html));

    for (size_t i = 0; i < g_project_count; i++) {
        char rel[256];
        snprintf(rel, sizeof(rel), "projects/%s/index.html", g_projects[i].slug);
        rc |= export_page(root, rel, page_project(&g_projects[i]));
    }
    for (size_t i = 0; i < g_guide_count; i++) {
        char rel[256];
        snprintf(rel, sizeof(rel), "guides/%s/index.html", g_guides[i].slug);
        rc |= export_page(root, rel, page_guide(&g_guides[i]));
    }

    cwist_sstring *xml = build_sitemap();
    rc |= write_file(root, "sitemap.xml", xml->data, strlen(xml->data));
    cwist_sstring_destroy(xml);

    rc |= write_file(root, "robots.txt", g_robots, strlen(g_robots));
    rc |= write_file(root, ".nojekyll", "", 0);

    /* The stylesheet goes out under the same content-hashed URL the pages
     * reference, so the static export and the live server stay identical. */
    const char *parts[] = { TPL_APP_CSS };
    cwist_sstring *css = cwist_css_bundle(parts, 1, true);
    const char *text = css && css->data ? css->data : TPL_APP_CSS;
    rc |= write_file(root, g_css_url + 1, text, strlen(text));
    rc |= write_file(root, "assets/css/app.css", text, strlen(text));
    if (css) cwist_sstring_destroy(css);

    (void)app;
    printf("site: export %s\n", rc == 0 ? "complete" : "finished with errors");
    return rc;
}

/* -------------------------------------------------------------------------- */
/* Entry point                                                                */
/* -------------------------------------------------------------------------- */

int main(int argc, char **argv) {
    cwist_app *app = cwist_app_create();
    if (!app) {
        fprintf(stderr, "site: cwist_app_create failed\n");
        return 1;
    }

    if (cwist_db_open(&g_db, ":memory:").error.err_i16 != 0 || !g_db) {
        fprintf(stderr, "site: cannot open the search index\n");
        cwist_app_destroy(app);
        return 1;
    }
    build_index();
    register_styles(app);

    if (argc > 2 && strcmp(argv[1], "--export") == 0) {
        int rc = export_site(app, argv[2]);
        cwist_db_close(g_db);
        cwist_app_destroy(app);
        return rc == 0 ? 0 : 1;
    }

    cwist_app_get(app, "/", h_home);
    cwist_app_get(app, "/projects", h_projects);
    cwist_app_get(app, "/projects/", h_projects);
    cwist_app_get(app, "/projects/:slug", h_project);
    cwist_app_get(app, "/projects/:slug/", h_project);
    cwist_app_get(app, "/guides", h_guides);
    cwist_app_get(app, "/guides/", h_guides);
    cwist_app_get(app, "/guides/:slug", h_guide);
    cwist_app_get(app, "/guides/:slug/", h_guide);
    cwist_app_get(app, "/about", h_about);
    cwist_app_get(app, "/about/", h_about);
    cwist_app_get(app, "/contribute", h_contribute);
    cwist_app_get(app, "/contribute/", h_contribute);
    cwist_app_get(app, "/search", h_search);
    cwist_app_get(app, "/search/", h_search);
    cwist_app_get(app, "/sitemap.xml", h_sitemap);
    cwist_app_get(app, "/robots.txt", h_robots);

    printf("c4punks: listening on http://localhost:%d\n", SITE_PORT);
    fflush(stdout);

    int rc = cwist_app_listen(app, SITE_PORT);

    cwist_db_close(g_db);
    cwist_app_destroy(app);
    return rc;
}

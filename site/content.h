/**
 * @file content.h
 * @brief Editorial content for the C 4 Punk Developers organisation site.
 *
 * Everything the site says about the organisation and its repositories lives
 * here as plain data, so the handlers in main.c only deal with routing,
 * templating and rendering.
 */

#ifndef SITE_CONTENT_H
#define SITE_CONTENT_H

#include <stddef.h>

#define SITE_ORG        "C 4 Punk Developers"
#define SITE_BASE       "https://c4punks.github.io"
#define SITE_GITHUB     "https://github.com/c4punks"
#define SITE_DISCORD    "https://discord.gg/6F8HDmNAPg"
#define SITE_YEAR       "2026"
#define SITE_BLURB \
    "An independent open-source group writing systems software in C. " \
    "We publish everything we build, and we keep the measurements honest."

typedef struct {
    const char *mark;
    const char *title;
    const char *body;
} feature_t;

typedef struct {
    const char *n;
    const char *l;
} stat_t;

typedef struct {
    const char *name;
    const char *body;
    const char *repo;
} satellite_t;

typedef struct {
    const char *slug;
    const char *name;
    const char *tag;
    const char *tagline;
    const char *summary;
    const char *repo;
    const char *licence;
    const char *licence_note;
    const char *bullets_html;
    const char *code_label;
    const char *code;
    const char *glance;
    const char *body_html;
    const feature_t *features;
    size_t feature_count;
} project_t;

typedef struct {
    const char *slug;
    const char *title;
    const char *summary;
    const char *project;
    const char *level;
    const char *minutes;
    const char *body_html;
} guide_t;

/* -------------------------------------------------------------------------- */
/* Organisation                                                               */
/* -------------------------------------------------------------------------- */

static const stat_t g_stats[] = {
    { "2",           "flagship projects" },
    { "C17",         "language standard" },
    { "MIT + BSD-3", "project licences" },
    { "8",           "dependencies vendored" }
};

static const feature_t g_principles[] = {
    { "01", "Plain C, no hidden runtime",
      "Our libraries are C17 and link statically. There is no VM to install, no "
      "interpreter to ship next to your binary, and no code generator between "
      "you and the source you are reading." },
    { "02", "Vendored dependencies",
      "A fresh checkout builds on a fresh machine. We vendor what we depend on "
      "so that a build does not become an archaeology exercise two years from now." },
    { "03", "Determinism over averages",
      "Tail latency, allocation behaviour and resource ownership are designed "
      "first. An average throughput number hides exactly the failures that page "
      "somebody at three in the morning." },
    { "04", "Benchmarks with their caveats attached",
      "We publish run conditions, commit hashes and the shape of the distribution, "
      "not only the headline number. Where a competitor wins, we say so in the "
      "same table." },
    { "05", "Readable by whoever operates it",
      "Code is reviewed for whether an on-call engineer can follow it at speed. "
      "Clever wins that cost legibility do not get merged." },
    { "06", "Public by default",
      "Design discussion, issues and roadmaps happen in the open repositories. "
      "There is no private fork where the real work happens." }
};

static const satellite_t g_satellites[] = {
    { "libttak-books",
      "The long-form book for libttak: memory model, scheduling and the reasoning "
      "behind the runtime's design decisions.",
      "https://github.com/c4punks/libttak-books" },
    { "homebrew-cwist",
      "The Homebrew tap. Builds CWIST from the release tarball and installs the "
      "static library, headers, CLI and pkg-config metadata.",
      "https://github.com/c4punks/homebrew-cwist" },
    { "c4punks.github.io",
      "This site. A CWIST application compiled to wasm32-wasip2 that both serves "
      "the site live and exports it as static HTML.",
      "https://github.com/c4punks/c4punks.github.io" }
};

static const char g_about_html[] =
    "<h2>Who we are</h2>"
    "<p>C 4 Punk Developers is a small, independent group of systems programmers. "
    "We build the parts of the stack that most teams would rather not think about: "
    "the HTTP server, the allocator, the scheduler, the reclamation strategy. We "
    "build them in C, we build them in public, and we ship them under permissive "
    "licences so that they can be used without a procurement conversation.</p>"

    "<h2>Why C, in this decade</h2>"
    "<p>The honest answer is control. When a service has to hold a predictable "
    "tail latency under load, the useful questions are about who owns a buffer, "
    "when it is reclaimed, and how many threads are parked waiting. C lets us "
    "answer those questions directly instead of arguing with a runtime about them. "
    "The cost is that we have to be disciplined, which is why our review rules are "
    "written down and applied to ourselves first.</p>"
    "<p>We are not claiming C is the right choice for every project. We are "
    "claiming that if you have decided to write C, the surrounding ecosystem "
    "should be as good as anyone else's: a framework that routes and "
    "speaks modern protocols, a runtime that makes memory behaviour explicit, "
    "and documentation that reads like it was written for a person.</p>"

    "<h2>What we publish</h2>"
    "<p><strong>CWIST</strong> is a web framework and application server. It "
    "speaks HTTP/1.1, HTTP/2 and HTTP/3 over QUIC, plus WebSocket and "
    "WebTransport, with hybrid post-quantum TLS and an embedded SQLite ORM. It "
    "ships two request paths, a multiplexing reactor and a "
    "thread-per-connection pool, because those two shapes fail differently "
    "under load and the right answer depends on the workload.</p>"
    "<p><strong>libttak</strong> is the deterministic systems runtime underneath "
    "it: generational arenas, epoch-based reclamation, detachable ownership, "
    "thread pools, a lattice scheduler and zero-copy IO. It exists because the "
    "usual C failure modes at scale (heap fragmentation, allocator "
    "contention, unstable tails) are not independent problems, and treating "
    "them as one system produces better behaviour than patching each in isolation.</p>"

    "<h2>How decisions get made</h2>"
    "<p>Proposals live in issues on the repository they affect. A change that "
    "alters public API or observable performance needs a written rationale and, "
    "where relevant, a measurement. Maintainers merge; anyone can review. There "
    "is no separate committee and no closed mailing list.</p>"

    "<h2>Licensing</h2>"
    "<p>Every repository carries its own <code>LICENSE</code> file, and that "
    "file is the authoritative text. This table is a convenience, not a "
    "warranty; check the repository you are about to depend on.</p>"
    "<table>"
    "<thead><tr><th>Repository</th><th>Licence</th><th>Copyright</th></tr></thead>"
    "<tbody>"
    "<tr><td>CWIST</td><td>MIT</td><td>2026 CWIST contributors</td></tr>"
    "<tr><td>libttak</td><td>BSD 3-Clause</td><td>2026 Religiya Serdtsa</td></tr>"
    "<tr><td>c4punks.github.io</td><td>MIT</td><td>2026 " SITE_ORG "</td></tr>"
    "</tbody>"
    "</table>"
    "<p>CWIST vendors its dependencies, and each one keeps its own terms. "
    "BoringSSL and cnats are Apache-2.0. lsquic, cJSON and multipart-parser-c "
    "are MIT, with some proto-quic-derived parts of lsquic under BSD-3-Clause. "
    "libttak and uriparser are BSD-3-Clause. SQLite is public domain. "
    "<code>NOTICE.md</code> in the CWIST repository lists all of them with "
    "their paths.</p>"
    "<p>Static linking is the part people miss. When you distribute "
    "<code>libcwist.a</code>, or a binary linked against it, the obligations of "
    "every component in that linked set travel with it: the Apache-2.0 patent "
    "grant and NOTICE terms, and the attribution clauses of the BSD-licensed "
    "components. Review the set before you ship rather than after.</p>"
    "<p>This site redistributes three web fonts, all under the SIL Open Font "
    "License 1.1: Inter, Space Grotesk and JetBrains Mono. They are embedded in "
    "the binary as latin subsets and served from the asset registry; "
    "<code>site/fonts/README.md</code> carries the notice the OFL requires.</p>"
    "<p>Contributions are accepted under the licence of the repository you are "
    "contributing to. We do not ask for a copyright assignment or a separate "
    "contributor licence agreement.</p>"

    "<h2>Talking to us</h2>"
    "<p>Bug reports and questions belong in the issue tracker of the relevant "
    "repository, where they stay searchable. For faster back-and-forth there is a "
    "Discord. Security reports should go to the maintainers privately first; each "
    "repository documents how.</p>";

static const char g_contribute_html[] =
    "<h2>You do not need permission</h2>"
    "<p>Fork the repository, make the change, open a pull request. If you want to "
    "check the direction first, open an issue and say what you intend to do. "
    "That is a courtesy, not a gate.</p>"

    "<h2>Good first contributions</h2>"
    "<ul>"
    "<li><strong>Reproduce a benchmark.</strong> Run one of our published numbers "
    "on your own hardware and tell us what you got. Divergence is information.</li>"
    "<li><strong>Documentation that unblocked you.</strong> If you had to read the "
    "source to work something out, that is a documentation bug; write down what "
    "you learned.</li>"
    "<li><strong>Platform reports.</strong> Build on a BSD, an older glibc, a "
    "musl container, an Apple Silicon Mac, and tell us what broke.</li>"
    "<li><strong>Hard bug reports.</strong> A crash with a reproducer is worth "
    "more to us than a feature request.</li>"
    "</ul>"

    "<h2>What a pull request should contain</h2>"
    "<ol>"
    "<li>A description of the problem, not only the fix. Reviewers need to be "
    "able to disagree with the framing.</li>"
    "<li>A test, or an explanation of why the change is not testable.</li>"
    "<li>For performance work: before and after numbers, the command used, and "
    "the machine they were taken on.</li>"
    "<li>Commit messages in English, imperative mood, one logical change each.</li>"
    "</ol>"

    "<h2>Code style</h2>"
    "<p>Follow the file you are editing. Across the organisation: C17, four-space "
    "indentation, no tabs, braces on the same line, <code>snake_case</code> for "
    "functions and types, a <code>cwist_</code> or <code>ttak_</code> prefix on "
    "anything exported. Public headers carry Doxygen comments describing "
    "ownership and failure modes. Who frees the pointer is part of the "
    "API, so write it down.</p>"

    "<h2>Review expectations</h2>"
    "<p>We review for correctness, ownership clarity, and whether an on-call "
    "engineer could follow the code under pressure. Expect questions about "
    "lifetimes. Expect to be asked for a measurement if you claim something is "
    "faster. None of that is personal.</p>"

    "<h2>Licensing your contribution</h2>"
    "<p>Whatever you send is accepted under the licence already on that "
    "repository: MIT for CWIST and for this site, BSD 3-Clause for libttak. "
    "There is no copyright assignment and no separate contributor licence "
    "agreement to sign. If you are adding third-party code, say so in the pull "
    "request and name its licence, because vendoring it changes what downstream "
    "users have to comply with.</p>"

    "<h2>Code of conduct</h2>"
    "<p>Be direct about code and decent about people. Harassment, and using the "
    "issue tracker to litigate something other than the software, will get you "
    "removed. Maintainers make that call.</p>";

#endif /* SITE_CONTENT_H */

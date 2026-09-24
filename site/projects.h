/**
 * @file projects.h
 * @brief Project entries shown on the organisation site.
 */

#ifndef SITE_PROJECTS_H
#define SITE_PROJECTS_H

#include "content.h"

static const feature_t g_cwist_features[] = {
    { "H3", "Modern protocols",
      "HTTP/1.1, HTTP/2 and HTTP/3 over QUIC in one server, plus WebSocket and "
      "WebTransport, with no proxy in front to terminate them for you." },
    { "PQ", "Post-quantum TLS",
      "Hybrid X25519MLKEM768 key agreement, available behind a single call so "
      "that turning it on is a configuration decision rather than a project." },
    { "DB", "Embedded SQLite ORM",
      "A SQLite-backed ORM ships in the box, with auto-detection for a local "
      "PostgreSQL, MySQL or MariaDB when one is present." },
    { "RX", "Two request paths",
      "A multiplexing reactor for throughput and connection count, and an opt-in "
      "thread-per-connection pool for median and p99 latency." },
    { "WA", "WASI 0.2 target",
      "The same source compiles to wasm32-wasip2 and owns its accept loop through "
      "wasi:sockets, so a CWIST binary runs under wasmtime unchanged." },
    { "UI", "Rendering without a frontend stack",
      "An HTML element builder, a scoped CSS composer, a small template engine "
      "and content-hashed static assets, so a server can answer with a finished "
      "page instead of a JSON envelope." }
};

static const feature_t g_libttak_features[] = {
    { "AR", "Generational arenas",
      "Batched allocation with timestamped generations and bulk reclamation, so "
      "cleanup happens at boundaries you chose rather than wherever the last "
      "free() landed." },
    { "EP", "Epoch reclamation",
      "Cross-thread memory reclamation without a global pause, for structures "
      "that readers touch while writers retire them." },
    { "OW", "Detachable ownership",
      "Memory can be handed between subsystems explicitly, which makes the "
      "lifetime question answerable by reading the call rather than the history." },
    { "TP", "Thread pools and futures",
      "Prioritised task submission that returns a future, with a deterministic "
      "lattice scheduler underneath for predictable ordering." },
    { "IO", "Zero-copy IO",
      "Ingress paths that avoid an intermediate copy, aimed at lock-free "
      "pipelines and routing layers." },
    { "DS", "Container and math toolbox",
      "Hash tables, pools, ring buffers, trees, B+ trees, priority queues, "
      "bigint, matrix and NTT, with optional CUDA, OpenCL and ROCm acceleration." }
};

static const char g_cwist_code[] =
    "#include &lt;cwist/app.h&gt;\n"
    "\n"
    "static void hello(cwist_http_request *req,\n"
    "                  cwist_http_response *res) {\n"
    "    (void)req;\n"
    "    cwist_sstring_assign(res-&gt;body, \"Hello from CWIST!\");\n"
    "}\n"
    "\n"
    "int main(void) {\n"
    "    cwist_app *app = cwist_app_create();\n"
    "    cwist_app_get(app, \"/\", hello);\n"
    "    cwist_app_listen(app, 8080);\n"
    "    cwist_app_destroy(app);\n"
    "    return 0;\n"
    "}\n";

static const char g_libttak_code[] =
    "#include &lt;ttak/mem/arena_helper.h&gt;\n"
    "\n"
    "ttak_arena_env_config_t cfg;\n"
    "ttak_arena_env_config_init(&amp;cfg);\n"
    "\n"
    "ttak_arena_env_t env;\n"
    "ttak_arena_env_init(&amp;env, &amp;cfg);\n"
    "\n"
    "ttak_arena_generation_t gen;\n"
    "ttak_arena_generation_begin(&amp;env, &amp;gen, 1);\n"
    "\n"
    "void *buf = ttak_arena_generation_claim(&amp;env, &amp;gen, 4096);\n"
    "/* ... use buf for the whole request ... */\n"
    "\n"
    "ttak_arena_generation_retire(&amp;env, &amp;gen);\n"
    "ttak_arena_env_destroy(&amp;env);\n";

static const project_t g_projects[] = {
    {
        "cwist",
        "CWIST",
        "C17",
        "C Web development Is Still Trustworthy",
        "A web framework and application server written in plain C. It terminates "
        "modern protocols itself, embeds a database layer, and links statically "
        "into a single binary you can copy onto a machine.",
        "https://github.com/c4punks/CWIST",
        "<li>HTTP/1.1, HTTP/2, HTTP/3 (QUIC), WebSocket, WebTransport</li>"
        "<li>Hybrid post-quantum TLS (X25519MLKEM768)</li>"
        "<li>Embedded SQLite ORM, plus RDBMS auto-detection</li>"
        "<li>Reactor and thread-per-connection modes</li>"
        "<li>Builds for native targets and for wasm32-wasip2</li>",
        "main.c",
        g_cwist_code,
        "C17 library and application server. Vendors BoringSSL, lsquic, libttak "
        "and SQLite3, so a plain build works on a fresh Linux, macOS or BSD "
        "machine. Installable from source or from the Homebrew tap.",

        "<h2>What CWIST is</h2>"
        "<p>CWIST is a C17 web framework and application server. It is not a "
        "binding over someone else's HTTP engine: the protocol stack, the "
        "reactor, the TLS configuration and the database layer are all part of "
        "the same static library, and the result is one binary with no runtime "
        "to install beside it.</p>"
        "<p>The name is an argument as much as an acronym. Writing a web service "
        "in C is usually treated as a mistake by default; CWIST exists to make "
        "the ordinary version of that job &mdash; routing, TLS, a database, "
        "metrics, a health endpoint &mdash; as short to write as it is in any "
        "other ecosystem.</p>"

        "<h2>Two server modes, and why both exist</h2>"
        "<p>CWIST ships two request paths tuned for opposite things, selected "
        "with one environment variable.</p>"
        "<p>The <strong>CWIST reactor</strong> is the default. It multiplexes "
        "many connections per event loop, so connection count is decoupled from "
        "thread count and a connection costs a reactor slot rather than a parked "
        "thread. It is the mode to pick when the number of open connections is "
        "the thing that grows.</p>"
        "<p>The <strong>classic pool</strong> is thread-per-connection, enabled "
        "with <code>CWIST_C1M_MODE=0</code>. Every connection gets its own "
        "thread, so no request waits behind another in a batch. In our published "
        "runs it leads through the median and p99; the crossover against the "
        "async comparison happens at the extreme tail.</p>"
        "<p>We publish the whole distribution rather than an average, because an "
        "average hides which requests were slow. The numbers come from one "
        "commit, one load profile and one CI environment, and the runner CPU "
        "model moves them more than most code changes do. Read them as a shape, "
        "not a guarantee.</p>"

        "<h2>Where it fits</h2>"
        "<p>CWIST is a reasonable choice when you already have C in the picture: "
        "an embedded system that grew an API, a numerical or media service whose "
        "core is C and whose HTTP layer should not double the deployment, or a "
        "service where the operators want to read every layer they are on call "
        "for. It is a poor choice if what you actually want is a large library "
        "ecosystem for business logic.</p>"

        "<h2>Installing</h2>"
        "<p>CWIST vendors its dependencies, so a clone and <code>make</code> is "
        "usually enough. There is also a Homebrew tap for macOS and Linuxbrew. "
        "The getting-started guide walks through both, then builds a first "
        "server.</p>",
        g_cwist_features,
        sizeof(g_cwist_features) / sizeof(g_cwist_features[0]),
        "CWIST C web framework application server HTTP/1.1 HTTP/2 HTTP/3 QUIC "
        "WebSocket WebTransport post-quantum TLS X25519MLKEM768 SQLite ORM "
        "reactor thread-per-connection wasm32-wasip2 WASI static linking C17"
    },
    {
        "libttak",
        "libttak",
        "Runtime",
        "Deterministic systems runtime for C",
        "A low-level runtime focused on predictable memory behaviour, "
        "high-throughput concurrency and deterministic resource scheduling. It is "
        "what CWIST stands on, and it is usable on its own.",
        "https://github.com/c4punks/libttak",
        "<li>Generational arenas with bulk reclamation</li>"
        "<li>Epoch-based reclamation without global pauses</li>"
        "<li>Detachable memory ownership</li>"
        "<li>Thread pools, futures and a lattice scheduler</li>"
        "<li>Zero-copy IO and a container toolbox</li>",
        "arena.c",
        g_libttak_code,
        "Deterministic runtime for C. Powers web frameworks, routing layers, "
        "lock-free ingress pipelines and containerised services. Documented at "
        "length in the companion libttak-books repository.",

        "<h2>Why libttak exists</h2>"
        "<p>C applications tend to fail at scale in a small number of predictable "
        "ways: the heap fragments, the allocator contends, the tail latency stops "
        "being stable, async coordination becomes difficult to follow, and "
        "resource ownership drifts between subsystems until nobody can say who "
        "frees what.</p>"
        "<p>libttak treats those as one problem rather than five. Memory "
        "allocation, network scheduling and concurrency are unified under "
        "deterministic control, so the lifetime of a buffer, the point at which "
        "it is reclaimed, and the thread that will touch it next are all "
        "answerable from the code in front of you.</p>"

        "<h2>The memory model</h2>"
        "<p><strong>Generational arenas</strong> batch allocations and reclaim "
        "them in bulk at a generation boundary you declare. Cleanup becomes an "
        "event you scheduled instead of a cost distributed across every free "
        "call.</p>"
        "<p><strong>Epoch reclamation</strong> handles the cross-thread case: a "
        "writer can retire a structure while readers are still inside it, without "
        "stopping the world to do so.</p>"
        "<p><strong>Detachable ownership</strong> makes the handover explicit. "
        "Memory moves between subsystems as a deliberate call, which is the "
        "difference between a lifetime bug you can find by reading and one you "
        "find in production.</p>"

        "<h2>Concurrency and scheduling</h2>"
        "<p>Thread pools take prioritised tasks and return futures. Underneath, a "
        "deterministic lattice scheduler orders work predictably rather than "
        "leaving it to whatever the OS decides this second. Zero-copy IO paths "
        "are aimed at lock-free ingress pipelines where an extra copy per packet "
        "is the whole budget.</p>"

        "<h2>What is built on it</h2>"
        "<p>libttak powers custom web frameworks &mdash; including CWIST &mdash; "
        "network routing layers, lock-free ingress pipelines, containerised "
        "services and experimental overlay networking. It is a library, not a "
        "framework: you call it from your own <code>main</code>.</p>",
        g_libttak_features,
        sizeof(g_libttak_features) / sizeof(g_libttak_features[0]),
        "libttak deterministic systems runtime C generational arena epoch "
        "reclamation detachable ownership thread pool future lattice scheduler "
        "zero-copy IO hash table ring buffer B+ tree priority queue bigint matrix "
        "NTT CUDA OpenCL ROCm tail latency allocator contention"
    }
};

static const size_t g_project_count = sizeof(g_projects) / sizeof(g_projects[0]);

#endif /* SITE_PROJECTS_H */

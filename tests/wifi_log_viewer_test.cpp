#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

extern "C" {
#include "pov_log.h"
#include "wifi_config/wifi_log_web.h"
}

static uint64_t s_now;
static uint64_t clock_ms() { return s_now; }

static size_t occurrences(const std::string &text, const std::string &token) {
    size_t count = 0, pos = 0;
    while ((pos = text.find(token, pos)) != std::string::npos) { ++count; pos += token.size(); }
    return count;
}

static std::string updates(bool has_session, uint64_t session,
                           uint32_t after, uint8_t limit) {
    char buf[8192];
    int n = wifi_log_web_build_updates(buf, sizeof(buf), has_session,
                                       session, after, limit);
    assert(n > 0);
    return std::string(buf, (size_t)n);
}

int main() {
    pov_log_init(0x1234u, clock_ms);
    std::string empty = updates(false, 0, 0, 16);
    assert(empty.find("\"entries\":[]") != std::string::npos);
    assert(empty.find("\"oldest_seq\":0") != std::string::npos);

    s_now = 42;
    pov_logf(POV_LOG_SOURCE_SYSTEM, "quote \" slash \\ newline\n");
    std::string initial = updates(false, 0, 0, 16);
    assert(initial.find("\"session\":\"0000000000001234\"") != std::string::npos);
    assert(initial.find("quote \\\" slash \\\\ newline") != std::string::npos);
    assert(initial.find("\"next_after\":1") != std::string::npos);

    std::string paused = updates(true, 0x1234u, 1, 0);
    assert(paused.find("\"entries\":[]") != std::string::npos);
    assert(paused.find("\"next_after\":1") != std::string::npos);

    for (unsigned i = 2; i <= 140; ++i) pov_logf(POV_LOG_SOURCE_DRIVER, "line %u", i);
    std::string stale = updates(true, 0x1234u, 1, 16);
    assert(stale.find("\"gap\":{\"first_missing\":2,\"last_missing\":12}") != std::string::npos);
    assert(stale.find("\"seq\":13") != std::string::npos);
    assert(stale.find("\"more\":true") != std::string::npos);
    assert(occurrences(stale, "\"seq\":") == 16);
    std::string page2 = updates(true, 0x1234u, 28, 16);
    assert(page2.find("\"seq\":29") != std::string::npos);
    assert(page2.find("\"seq\":28,") == std::string::npos);

    std::string restarted = updates(true, 0x9999u, 100, 16);
    assert(restarted.find("\"session_changed\":true") != std::string::npos);
    assert(restarted.find("\"seq\":13") != std::string::npos);

    char buf[1024];
    assert(wifi_log_web_build_updates(buf, sizeof(buf), true, 0x1234u,
                                      141, 16) == WIFI_LOG_JSON_INVALID_CURSOR);
    assert(wifi_log_web_build_updates(buf, sizeof(buf), false, 0, 0,
                                      17) < 0);
    char tiny[32];
    assert(wifi_log_web_build_updates(tiny, sizeof(tiny), false, 0, 0, 16) < 0);

    pov_log_init(0x7777u, clock_ms);
    std::string empty_restart = updates(true, 0x1234u, 140, 16);
    assert(empty_restart.find("\"session_changed\":true") != std::string::npos);
    assert(empty_restart.find("\"entries\":[]") != std::string::npos);
    pov_logf(POV_LOG_SOURCE_SYSTEM, "<tag>& snow-\xE9\x9B\xAA");
    std::string markup = updates(false, 0, 0, 16);
    assert(markup.find("<tag>&") == std::string::npos);
    assert(markup.find("\\u003ctag\\u003e\\u0026") != std::string::npos);

    char page[16384];
    int page_n = wifi_log_web_build_page(page, sizeof(page));
    assert(page_n > 0);
    std::string html(page, (size_t)page_n);
    assert(html.find("/logs/updates") != std::string::npos);
    assert(html.find("textContent") != std::string::npos);
    assert(html.find("Pause") != std::string::npos);
    assert(html.find("Session --") != std::string::npos);
    assert(html.find("Uptime --") != std::string::npos);
    assert(html.find("No log entries yet.") != std::string::npos);
    assert(html.find("[truncated]") != std::string::npos);
    assert(html.find("pov-theme") != std::string::npos);
    assert(html.find("Toggle color theme") == std::string::npos);
    assert(html.find("id=theme") == std::string::npos);
    assert(html.find("id=clear") != std::string::npos);
    assert(html.find("Clear displayed logs") != std::string::npos);
    assert(html.find("cb.onclick=()=>{out.textContent='';out.scrollTop=0}") !=
           std::string::npos);
    assert(html.find("AbortController") != std::string::npos);
    assert(html.find("class=active href=/logs") != std::string::npos);
    assert(html.find("j.more&&!paused?30:1000") != std::string::npos);
    assert(html.find("Disconnected; retrying") != std::string::npos);
    assert(occurrences(html, "fetch(") == 1);
    assert(html.find("innerHTML") == std::string::npos);
    assert(html.find("Math.min(retry*2,5000)") != std::string::npos);
    assert(html.find("Invalid log order") != std::string::npos);
    assert(html.find("Unexpected log session") != std::string::npos);
    assert(html.find("pad(ms%1000,3)") != std::string::npos);
    assert(html.find("else if(metadataOnly)") != std::string::npos);
    assert(html.find("if(!paused&&!inflight)schedule(0)") != std::string::npos);
    assert(html.find("Paused; gap") != std::string::npos);
    assert(html.find("Math.max(0,j.newest_seq-after)") != std::string::npos);
    assert(html.find("while(out.children.length>128)") != std::string::npos);
    assert(wifi_log_web_build_page(tiny, sizeof(tiny)) < 0);
    assert(page_n < 16384);

    std::cout << "wifi log viewer tests passed (page bytes=" << page_n << ", max batch bytes=" << stale.size() << ")\n";
}


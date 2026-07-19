#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "pov_log.h"

namespace {

uint64_t fake_now;

uint64_t fake_clock() { return fake_now; }

void test_empty_and_first_entry() {
    fake_now = 12u;
    pov_log_init(0u, fake_clock);
    pov_log_snapshot_t snapshot = pov_log_snapshot();
    assert(snapshot.boot_id == 1u);
    assert(snapshot.count == 0u);
    assert(snapshot.oldest_sequence == 0u);
    assert(snapshot.newest_sequence == 0u);
    assert(snapshot.uptime_ms == 12u);

    fake_now = 34u;
    pov_logf(POV_LOG_SOURCE_HALL, "rpm=%u", 600u);
    snapshot = pov_log_snapshot();
    assert(snapshot.count == 1u);
    assert(snapshot.oldest_sequence == 1u);
    assert(snapshot.newest_sequence == 1u);

    pov_log_entry_t entry{};
    assert(pov_log_read(1u, &entry));
    assert(entry.sequence == 1u);
    assert(entry.uptime_ms == 34u);
    assert(entry.source == POV_LOG_SOURCE_HALL);
    assert(std::strcmp(entry.text, "rpm=600") == 0);
    assert(entry.text_len == std::strlen(entry.text));
    assert(!pov_log_read(0u, &entry));
    assert(!pov_log_read(2u, &entry));
}

void test_overwrite_and_order() {
    pov_log_init(0x123456789abcdef0ULL, fake_clock);
    for (uint32_t i = 1u; i <= 1000u; ++i) {
        fake_now = i * 10u;
        pov_logf(POV_LOG_SOURCE_CLOCK, "entry-%u", i);
    }

    pov_log_snapshot_t snapshot = pov_log_snapshot();
    assert(snapshot.boot_id == 0x123456789abcdef0ULL);
    assert(snapshot.count == POV_LOG_CAPACITY);
    assert(snapshot.oldest_sequence == 873u);
    assert(snapshot.newest_sequence == 1000u);

    for (uint32_t sequence = snapshot.oldest_sequence;
         sequence <= snapshot.newest_sequence; ++sequence) {
        pov_log_entry_t entry{};
        assert(pov_log_read(sequence, &entry));
        assert(entry.sequence == sequence);
        assert(entry.uptime_ms == (uint64_t)sequence * 10u);
    }
}

void test_truncation_utf8_and_controls() {
    pov_log_init(7u, fake_clock);
    std::string long_text(140u, 'A');
    long_text += "\xE7\x95\x8C";
    pov_logf(POV_LOG_SOURCE_SYSTEM, "%s", long_text.c_str());

    pov_log_entry_t entry{};
    assert(pov_log_read(1u, &entry));
    assert(entry.flags & POV_LOG_FLAG_TRUNCATED);
    assert(entry.text_len == POV_LOG_TEXT_MAX);
    assert(std::strcmp(entry.text + entry.text_len - 3u, "...") == 0);
    assert(entry.text[entry.text_len] == '\0');

    std::string exact(100u, 'B');
    pov_logf(POV_LOG_SOURCE_SYSTEM, "%s\n", exact.c_str());
    assert(pov_log_read(2u, &entry));
    assert(!(entry.flags & POV_LOG_FLAG_TRUNCATED));
    assert(entry.text_len == 100u);


    const char invalid[] = {'o', 'k', '\n', (char)0xff, 'x', '\0'};
    pov_logf(POV_LOG_SOURCE_SYSTEM, "%s", invalid);
    assert(pov_log_read(3u, &entry));
    assert(!(entry.flags & POV_LOG_FLAG_SANITIZED));
    assert(std::strcmp(entry.text, "ok??x") == 0);
}

void test_sensitive_redaction() {
    pov_log_init(8u, fake_clock);
    pov_logf(POV_LOG_SOURCE_WIFI_HTTP,
             "ssid=lab&password=unique-secret&admin_token=token-secret");
    pov_log_entry_t entry{};
    assert(pov_log_read(1u, &entry));
    assert(entry.flags & POV_LOG_FLAG_SANITIZED);
    assert(std::strstr(entry.text, "unique-secret") == nullptr);
    assert(std::strstr(entry.text, "token-secret") == nullptr);
    assert(std::strstr(entry.text, "redacted") != nullptr);

    pov_logf(POV_LOG_SOURCE_WIFI_STA_HTTP, "pw_len=%u token_len=%u", 12u, 16u);
    assert(pov_log_read(2u, &entry));

    pov_logf(POV_LOG_SOURCE_WIFI_HTTP, "Authorization: Bearer unique-auth-secret");
    assert(pov_log_read(3u, &entry));
    assert(entry.flags & POV_LOG_FLAG_SANITIZED);
    assert(std::strstr(entry.text, "unique-auth-secret") == nullptr);
    pov_logf(POV_LOG_SOURCE_WIFI_HTTP, "X-Admin-Token: unique-admin-secret");
    assert(pov_log_read(4u, &entry));
    assert(entry.flags & POV_LOG_FLAG_SANITIZED);
    assert(std::strstr(entry.text, "unique-admin-secret") == nullptr);
}

void test_session_reset_sources_and_budget() {
    pov_log_init(21u, fake_clock);
    pov_logf(POV_LOG_SOURCE_UPDATE, "ready");
    assert(std::strcmp(pov_log_source_text(POV_LOG_SOURCE_UPDATE), "update") == 0);
    assert(std::strcmp(pov_log_source_text((pov_log_source_t)255), "system") == 0);
    assert(sizeof(pov_log_entry_t) == POV_LOG_ENTRY_TARGET_BYTES);
    assert(pov_log_static_bytes() <= POV_LOG_STATE_MAX_BYTES);

    pov_log_init(22u, fake_clock);
    pov_log_snapshot_t snapshot = pov_log_snapshot();
    assert(snapshot.boot_id == 22u);
    assert(snapshot.count == 0u);
}

}  // namespace

int main() {
    test_empty_and_first_entry();
    test_overwrite_and_order();
    test_truncation_utf8_and_controls();
    test_sensitive_redaction();
    test_session_reset_sources_and_budget();
    std::puts("pov log tests passed");
    return 0;
}

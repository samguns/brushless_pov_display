#include "wifi_log_web.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pov_log.h"

typedef struct {
    char *buf;
    size_t cap;
    size_t len;
    bool ok;
} writer_t;

static void appendf(writer_t *w, const char *fmt, ...) {
    if (!w->ok || w->len >= w->cap) return;
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(w->buf + w->len, w->cap - w->len, fmt, args);
    va_end(args);
    if (n < 0 || (size_t)n >= w->cap - w->len) {
        w->ok = false;
        return;
    }
    w->len += (size_t)n;
}

static void append_bytes(writer_t *w, const char *data, size_t length) {
    if (!w->ok || length >= w->cap - w->len) {
        w->ok = false;
        return;
    }
    memcpy(w->buf + w->len, data, length);
    w->len += length;
    w->buf[w->len] = '\0';
}

static void append_json_string(writer_t *w, const char *s) {
    append_bytes(w, "\"", 1u);
    for (const unsigned char *p = (const unsigned char *)s; w->ok && *p; ++p) {
        switch (*p) {
            case '\"': append_bytes(w, "\\\"", 2u); break;
            case '\\': append_bytes(w, "\\\\", 2u); break;
            case '\b': append_bytes(w, "\\b", 2u); break;
            case '\f': append_bytes(w, "\\f", 2u); break;
            case '\n': append_bytes(w, "\\n", 2u); break;
            case '\r': append_bytes(w, "\\r", 2u); break;
            case '\t': append_bytes(w, "\\t", 2u); break;
            case '<': append_bytes(w, "\\u003c", 6u); break;
            case '>': append_bytes(w, "\\u003e", 6u); break;
            case '&': append_bytes(w, "\\u0026", 6u); break;
            default:
                if (*p < 0x20u) appendf(w, "\\u%04x", (unsigned)*p);
                else append_bytes(w, (const char *)p, 1u);
                break;
        }
    }
    append_bytes(w, "\"", 1u);
}

int wifi_log_web_build_updates(char *buf, size_t buflen,
                               bool has_session, uint64_t session,
                               uint32_t after, uint8_t limit) {
    if (!buf || buflen == 0u || limit > WIFI_LOG_BATCH_MAX) return -1;

    pov_log_snapshot_t snap = pov_log_snapshot();
    bool restarted = has_session && session != snap.boot_id;
    bool gap = false;
    uint32_t gap_from = 0u;
    uint32_t gap_to = 0u;
    uint32_t cursor = after;

    if (has_session && !restarted && after > snap.newest_sequence) {
        return WIFI_LOG_JSON_INVALID_CURSOR;
    }
    if (!has_session || restarted) {
        cursor = snap.count ? snap.oldest_sequence - 1u : 0u;
        if (snap.count && snap.oldest_sequence > 1u) {
            gap = true;
            gap_from = 1u;
            gap_to = snap.oldest_sequence - 1u;
        }
    } else if (snap.count && after < snap.oldest_sequence - 1u) {
        gap = true;
        gap_from = after + 1u;
        gap_to = snap.oldest_sequence - 1u;
        cursor = snap.oldest_sequence - 1u;
    }

    uint32_t available = snap.newest_sequence > cursor
                             ? snap.newest_sequence - cursor : 0u;
    uint32_t emit = available < limit ? available : limit;
    uint32_t next_after = cursor;

    writer_t w = {buf, buflen, 0u, true};
    appendf(&w,
            "{\"session\":\"%016llx\",\"uptime_ms\":%llu,"
            "\"oldest_seq\":%u,\"newest_seq\":%u,\"session_changed\":%s,\"gap\":",
            (unsigned long long)snap.boot_id,
            (unsigned long long)snap.uptime_ms,
            (unsigned)snap.oldest_sequence, (unsigned)snap.newest_sequence,
            restarted ? "true" : "false");
    if (gap) {
        appendf(&w, "{\"first_missing\":%u,\"last_missing\":%u}",
                (unsigned)gap_from, (unsigned)gap_to);
    } else {
        appendf(&w, "null");
    }
    appendf(&w, ",\"entries\":[");

    for (uint32_t i = 0u; i < emit; ++i) {
        pov_log_entry_t entry;
        uint32_t seq = cursor + i + 1u;
        if (!pov_log_read(seq, &entry)) break;
        if (i) appendf(&w, ",");
        appendf(&w, "{\"seq\":%u,\"time_ms\":%llu,\"source\":",
                (unsigned)entry.sequence, (unsigned long long)entry.uptime_ms);
        append_json_string(&w, pov_log_source_text((pov_log_source_t)entry.source));
        appendf(&w, ",\"message\":");
        append_json_string(&w, entry.text);
        appendf(&w, ",\"truncated\":%s}",
                (entry.flags & POV_LOG_FLAG_TRUNCATED) ? "true" : "false");
        next_after = entry.sequence;
    }

    appendf(&w, "],\"next_after\":%u,\"more\":%s}",
            (unsigned)next_after,
            snap.newest_sequence > next_after ? "true" : "false");
    return w.ok ? (int)w.len : -1;
}

int wifi_log_web_build_page(char *buf, size_t buflen) {
    if (!buf || buflen == 0u) return -1;
    writer_t w = {buf, buflen, 0u, true};
    appendf(&w,
        "<!doctype html><html lang=en><head><meta charset=utf-8>"
        "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>Logs - POV Display</title><style>"
        ":root{--bg:#0d0f14;--side:#0a0c11;--card:#13161e;--line:#272b35;"
        "--text:#e2e4e9;--muted:#89909e;--accent:#2dd4bf;--danger:#ef4444}"
        ":root[data-theme=light]{--bg:#f4f5f7;--side:#fff;--card:#fff;--line:#d7dbe2;"
        "--text:#1a1d27;--muted:#6b7280;--accent:#0d9488;--danger:#dc2626}"
        "*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);"
        "font:14px system-ui,-apple-system,Segoe UI,sans-serif}.app{display:flex;min-height:100vh}"
        "aside{width:196px;background:var(--side);border-right:1px solid var(--line);padding:14px 10px}"
        ".brand{font-weight:700;padding:8px;color:var(--accent)}nav{margin-top:18px}"
        "nav a{display:block;padding:9px 10px;margin:3px 0;border-radius:6px;color:var(--muted);text-decoration:none}"
        "nav a.active{color:var(--accent);background:#17312f}main{flex:1;min-width:0;padding:24px}"
        "h1{font-size:20px;margin:0}.sub{color:var(--muted);margin:5px 0 18px}"
        ".bar{display:flex;align-items:center;gap:10px;flex-wrap:wrap;margin-bottom:12px}"
        "button{background:var(--card);border:1px solid var(--line);color:var(--text);padding:7px 12px;border-radius:5px;cursor:pointer}"
        ".status{font:12px ui-monospace,monospace;color:var(--accent)}"
        ".log{height:calc(100vh - 145px);min-height:320px;overflow:auto;background:#080a0e;"
        "border:1px solid var(--line);border-radius:7px;padding:10px;font:12px/1.55 ui-monospace,SFMono-Regular,Consolas,monospace}"
        ".line{white-space:pre-wrap;overflow-wrap:anywhere}.meta{color:#6f7787}.src{color:var(--accent)}"
        ".marker{color:#fbbf24}.error{color:var(--danger)}"
        "@media(max-width:640px){.app{display:block}aside{width:100%%;padding:8px}nav{margin:4px 0;display:flex}"
        "main{padding:14px}.log{height:65vh}}"
        "</style><script>(function(){try{if(localStorage.getItem('pov-theme')==='light')"
        "document.documentElement.setAttribute('data-theme','light')}catch(e){}})()</script>"
        "</head><body><div class=app><aside><div class=brand>POV Display</div><nav>"
        "<a href=/>Overview</a><a href=/settings>Settings</a><a class=active href=/logs>Logs</a>"
        "</nav></aside><main><h1>Device logs</h1><p class=sub>Live current-boot diagnostics over Wi-Fi</p>"
        "<div class=bar><button id=pause>Pause</button><button id=clear aria-label=\"Clear displayed logs\">Clear</button>"
        "<span class=status id=status>Connecting...</span><span class=status id=session>Session --</span>"
        "<span class=status id=uptime>Uptime --</span></div><div class=log id=log aria-live=polite></div>"
        "<script>"
        "(()=>{const out=document.getElementById('log'),st=document.getElementById('status'),"
        "pb=document.getElementById('pause'),sess=document.getElementById('session'),"
        "up=document.getElementById('uptime'),cb=document.getElementById('clear');"
        "let session='',after=0,paused=false,retry=1000,ctl,nextTimer=0,inflight=false;"
        "const schedule=ms=>{clearTimeout(nextTimer);nextTimer=setTimeout(poll,ms)};"
        "const row=(text,cls='marker')=>{const d=document.createElement('div');d.className='line '+cls;"
        "d.textContent=text;out.appendChild(d);while(out.children.length>128)out.firstChild.remove();"
        "out.scrollTop=out.scrollHeight};"
        "const showEmpty=()=>{if(!document.getElementById('empty')){const d=document.createElement('div');"
        "d.id='empty';d.className='line meta';d.textContent='No log entries yet.';out.appendChild(d)}};"
        "const pad=(n,w)=>String(n).padStart(w,'0');"
        "const stamp=ms=>{const q=Math.floor(ms/1000),h=Math.floor(q/3600),m=Math.floor(q/60)%%60,s=q%%60;"
        "return pad(h,2)+':'+pad(m,2)+':'+pad(s,2)+'.'+pad(ms%%1000,3)};"
        "const entry=e=>{const z=document.getElementById('empty');if(z)z.remove();"
        "const d=document.createElement('div');d.className='line';"
        "const m=document.createElement('span');m.className='meta';m.textContent='['+stamp(e.time_ms)+' #'+e.seq+'] ';"
        "const s=document.createElement('span');s.className='src';s.textContent=e.source+' ';"
        "const t=document.createTextNode(e.message+(e.truncated?' [truncated]':''));d.append(m,s,t);out.appendChild(d);"
        "while(out.children.length>128)out.firstChild.remove();out.scrollTop=out.scrollHeight};"
        "async function poll(){if(inflight)return;inflight=true;const metadataOnly=paused,requestAfter=after,requestSession=session;"
        "ctl=new AbortController();const timer=setTimeout(()=>ctl.abort(),4000);"
        "try{const q=new URLSearchParams({after:String(requestAfter),limit:metadataOnly?'0':'16'});"
        "if(requestSession)q.set('session',requestSession);"
        "const r=await fetch('/logs/updates?'+q,{cache:'no-store',signal:ctl.signal});if(!r.ok)throw Error('HTTP '+r.status);"
        "const j=await r.json();if(!/^[0-9a-f]{16}$/.test(j.session))throw Error('Invalid log session');"
        "if(requestSession&&!j.session_changed&&j.session!==requestSession)throw Error('Unexpected log session');"
        "if(requestSession&&j.session_changed&&j.session===requestSession)throw Error('Invalid session change');"
        "if(j.session_changed){out.textContent='';row('--- device restarted; starting session '+j.session+' ---');"
        "after=j.oldest_seq?j.oldest_seq-1:0}"
        "session=j.session;const pausedNow=paused;"
        "if(!pausedNow&&!metadataOnly){let expected=j.gap?j.gap.last_missing+1:after+1;"
        "for(const e of j.entries){if(e.seq!==expected)throw Error('Invalid log order');expected++}"
        "const rendered=j.entries.length?expected-1:after;if(j.next_after!==rendered)throw Error('Invalid log cursor');"
        "if(j.gap)row('--- logs overwritten: missing '+j.gap.first_missing+'..'+j.gap.last_missing+' ---');"
        "j.entries.forEach(entry);after=j.next_after}"
        "else if(metadataOnly){if(j.entries.length)throw Error('Invalid metadata response');"
        "if(!requestSession)after=j.next_after}"
        "if(j.newest_seq===0)showEmpty();retry=1000;"
        "sess.textContent='Session '+j.session;up.textContent='Uptime '+(j.uptime_ms/1000).toFixed(1)+'s';"
        "st.textContent=paused?(j.gap?'Paused; gap '+j.gap.first_missing+'..'+j.gap.last_missing:"
        "'Paused; '+Math.max(0,j.newest_seq-after)+' unseen'):'Live; '+j.newest_seq+' newest';"
        "schedule(j.more&&!paused?30:1000)}catch(e){st.textContent='Disconnected; retrying';"
        "schedule(retry);retry=Math.min(retry*2,5000)}finally{clearTimeout(timer);inflight=false}}"
        "pb.onclick=()=>{paused=!paused;pb.textContent=paused?'Resume':'Pause';"
        "st.textContent=paused?'Paused':'Resuming';if(!paused&&!inflight)schedule(0)};"
        "cb.onclick=()=>{out.textContent='';out.scrollTop=0};poll()})();"
        "</script></main></div></body></html>");
    return w.ok ? (int)w.len : -1;
}

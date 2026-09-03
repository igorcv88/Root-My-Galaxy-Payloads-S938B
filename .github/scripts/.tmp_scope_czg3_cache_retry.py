from pathlib import Path

fops = Path("src/fops.c")
text = fops.read_text()
old = '''#if defined(APP_PHYS_P0_ORACLE) && APP_PHYS_P0_ORACLE
      pipebuf_page_base = prepare_pipe_buffer_page();
      pr_info("fresh physrw cache-gate retry page attempt=%d/%d base=%016zx\\n",
              attempt + 1, PIPE_MAX_ATTEMPTS, pipebuf_page_base);
      if (!is_direct_ptr(pipebuf_page_base)) {
        continue;
      }
#elif defined(APP_FOPS_BEFORE_PIPE) && APP_FOPS_BEFORE_PIPE
'''
new = '''#if defined(APP_CZG3_CACHE_GATE_RECLAIM_RETRY) && APP_CZG3_CACHE_GATE_RECLAIM_RETRY
      pipebuf_page_base = prepare_pipe_buffer_page();
      pr_info("fresh physrw cache-gate retry page attempt=%d/%d base=%016zx\\n",
              attempt + 1, PIPE_MAX_ATTEMPTS, pipebuf_page_base);
      if (!is_direct_ptr(pipebuf_page_base)) {
        continue;
      }
#elif defined(APP_FOPS_BEFORE_PIPE) && APP_FOPS_BEFORE_PIPE
'''
if text.count(old) != 1:
    raise SystemExit(f"reprepare guard: expected 1 match, found {text.count(old)}")
text = text.replace(old, new, 1)
old = '''    /* A fresh reclaim is safe only while the slab-cache gate is still the
     * failing boundary. Once a candidate pipe slab was accepted or any
     * physrw proof began, keep the existing fail-closed behavior. */
    int safe_cache_gate_retry =
        !pipe_cache_gate_ok && !physrw_read_ok && !physrw_write_ok &&
        !physrw_read64_ok && !physrw_write64_ok;
    if (!safe_cache_gate_retry) {
      break;
    }
    if (attempt + 1 < PIPE_MAX_ATTEMPTS) {
      pr_info("physrw cache gate miss; retrying with a fresh reclaim attempt=%d/%d\\n",
              attempt + 2, PIPE_MAX_ATTEMPTS);
    }
'''
new = '''#if defined(APP_CZG3_CACHE_GATE_RECLAIM_RETRY) && APP_CZG3_CACHE_GATE_RECLAIM_RETRY
    /* A fresh reclaim is safe only while the slab-cache gate is still the
     * failing boundary. Once a candidate pipe slab was accepted or any
     * physrw proof began, keep the existing fail-closed behavior. */
    int safe_cache_gate_retry =
        !pipe_cache_gate_ok && !physrw_read_ok && !physrw_write_ok &&
        !physrw_read64_ok && !physrw_write64_ok;
    if (!safe_cache_gate_retry) {
      break;
    }
    if (attempt + 1 < PIPE_MAX_ATTEMPTS) {
      pr_info("physrw cache gate miss; retrying with a fresh reclaim attempt=%d/%d\\n",
              attempt + 2, PIPE_MAX_ATTEMPTS);
    }
#else
    if (pipe_cache_gate_ok && physrw_read_ok && physrw_write_ok &&
        physrw_read64_ok && physrw_write64_ok) {
      break;
    }
#endif
'''
if text.count(old) != 1:
    raise SystemExit(f"safe retry block: expected 1 match, found {text.count(old)}")
fops.write_text(text.replace(old, new, 1))

target = Path("src/targets/pa3q-S938BXXSBCZG3/target.h")
text = target.read_text()
old = '''#define APP_CZG3_DIAGNOSTICS 1
/* Two independent CZG3 boots reached verified FOPS but missed the
 * pipe slab cache gate before physrw began. Permit exactly one fresh
 * reclaim at that safe boundary; later stages remain fail-closed. */
#define PIPE_MAX_ATTEMPTS 2
'''
new = '''#define APP_CZG3_DIAGNOSTICS 1
#if defined(APP_PAYLOAD) && APP_PAYLOAD
/* Two independent CZG3 boots reached verified FOPS but missed the
 * pipe slab cache gate before physrw began. Permit exactly one fresh
 * reclaim at that safe boundary; later stages remain fail-closed. */
#define APP_CZG3_CACHE_GATE_RECLAIM_RETRY 1
#define PIPE_MAX_ATTEMPTS 2
#endif
'''
if text.count(old) != 1:
    raise SystemExit(f"target block: expected 1 match, found {text.count(old)}")
target.write_text(text.replace(old, new, 1))

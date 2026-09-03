from pathlib import Path

fops = Path("src/fops.c")
text = fops.read_text()
start_marker = "  int installed = 0;\n  pipe_stage_attempts = 0;\n  for (int attempt = 0; attempt < PIPE_MAX_ATTEMPTS; attempt++) {\n"
end_marker = "\n\n  if (!installed) {"
start = text.find(start_marker)
if start < 0:
    raise SystemExit("fops retry block start not found")
end = text.find(end_marker, start)
if end < 0:
    raise SystemExit("fops retry block end not found")
new = r'''  int installed = 0;
  pipe_stage_attempts = 0;
  for (int attempt = 0; attempt < PIPE_MAX_ATTEMPTS; attempt++) {
    pipe_stage_attempts++;
    if (attempt != 0) {
      reset_pipe_attempt();
#if defined(APP_PHYS_P0_ORACLE) && APP_PHYS_P0_ORACLE
      pipebuf_page_base = prepare_pipe_buffer_page();
      pr_info("fresh physrw cache-gate retry page attempt=%d/%d base=%016zx\n",
              attempt + 1, PIPE_MAX_ATTEMPTS, pipebuf_page_base);
      if (!is_direct_ptr(pipebuf_page_base)) {
        continue;
      }
#elif defined(APP_FOPS_BEFORE_PIPE) && APP_FOPS_BEFORE_PIPE
      pipebuf_page_base = prepare_pipe_buffer_page();
      pr_info("fresh physrw retry page attempt=%d/%d base=%016zx\n",
              attempt + 1, PIPE_MAX_ATTEMPTS, pipebuf_page_base);
      if (!is_direct_ptr(pipebuf_page_base)) {
        continue;
      }
#endif
    }
    if (install_child_root(fd)) {
      installed = 1;
      break;
    }

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
      pr_info("physrw cache gate miss; retrying with a fresh reclaim attempt=%d/%d\n",
              attempt + 2, PIPE_MAX_ATTEMPTS);
    }
  }'''
fops.write_text(text[:start] + new + text[end:])

target = Path("src/targets/pa3q-S938BXXSBCZG3/target.h")
text = target.read_text()
anchor = "#define APP_CZG3_DIAGNOSTICS 1\n"
addition = '''#define APP_CZG3_DIAGNOSTICS 1
/* Two independent CZG3 boots reached verified FOPS but missed the
 * pipe slab cache gate before physrw began. Permit exactly one fresh
 * reclaim at that safe boundary; later stages remain fail-closed. */
#define PIPE_MAX_ATTEMPTS 2
'''
if text.count(anchor) != 1:
    raise SystemExit("target diagnostics anchor not found exactly once")
target.write_text(text.replace(anchor, addition, 1))

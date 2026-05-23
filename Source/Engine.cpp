// ============================================================
//  Engine.cpp  —  sed execution engine
// ============================================================

#include "fastsed/Engine.hpp"
#include "fastsed/LineSource.hpp"

namespace fastsed {

// ── init ─────────────────────────────────────────────────────
void Engine::init(int ncmds) {
    total_cmds = ncmds;
    range_at.assign(ncmds, -1L);
}

// ── Address helpers ───────────────────────────────────────────
bool Engine::addr_single(const Addr& a) const {
    switch (a.k) {
    case AK::None:  return false;
    case AK::Last:  return last_line;
    case AK::Line:  return lineno == a.line;
    case AK::Line0: return lineno <= 1;
    case AK::Step:
        return a.step == 0 ? (lineno == a.first)
             : (lineno >= a.first && (lineno - a.first) % a.step == 0);
    case AK::RE: {
        const RE* re = (a.re && a.re->ok) ? a.re.get() : last_re.get();
        return re && re->test(ps.c_str());
    }
    }
    return false;
}

bool Engine::matches(const FlatCmd& c) {
    bool result;

    if (c.naddr == 0) {
        result = true;
    } else if (c.naddr == 1) {
        result = addr_single(c.a[0]);
    } else {
        long& entered = range_at[c.id];

        if (entered < 0) {
            // Not in range — test addr1
            bool m1 = (c.a[0].k == AK::Line0) ? (lineno <= 1)
                                                : addr_single(c.a[0]);
            if (m1) {
                entered = lineno;
                // Immediate closure: addr2 is a line number already passed
                if (c.a[1].k == AK::Line && c.a[1].line < lineno)
                    entered = -1;
                result = true;
            } else {
                result = false;
            }
        } else {
            // In range — test addr2
            bool close = false;
            switch (c.a[1].k) {
            case AK::Line:  close = (lineno >= c.a[1].line); break;
            case AK::Last:  close = last_line; break;
            case AK::Line0: close = true; break;
            case AK::RE:
                // addr2 regex is NOT checked on the line addr1 matched
                if (lineno > entered) close = addr_single(c.a[1]);
                break;
            default: break;
            }
            if (close) entered = -1;
            result = true;
        }
    }
    return c.neg ? !result : result;
}

// After matches(): range_at[id]==-1 means range just closed this line.
bool Engine::range_ending(const FlatCmd& c) const {
    if (c.naddr < 2) return true;
    return range_at[c.id] < 0;
}

// ── Output helpers ────────────────────────────────────────────
void Engine::write_to_file(const string& path, string_view sv) {
    auto it = wfiles.find(path);
    FILE* fp;
    if (it == wfiles.end()) {
        fp = fopen(path.c_str(), "w");
        if (!fp) {
            warn(std::format("cannot open '{}': {}", path, strerror(errno)));
            wfiles[path] = nullptr;
            return;
        }
        wfiles[path] = fp;
    } else {
        fp = it->second;
    }
    if (fp) { fwrite(sv.data(), 1, sv.size(), fp); fputc('\n', fp); }
}

void Engine::flush_deferred() {
    for (const Deferred& d : deferred) {
        if (!d.is_file) {
            out->writeln(d.val);
        } else {
            FILE* fp = fopen(d.val.c_str(), "r");
            if (!fp) continue;
            if (d.one_line) {
                char buf[4096];
                if (fgets(buf, sizeof buf, fp)) {
                    string_view sv{buf};
                    while (!sv.empty() && sv.back() == '\n') sv.remove_suffix(1);
                    out->writeln(sv);
                }
            } else {
                char buf[4096]; size_t nr;
                while ((nr = fread(buf, 1, sizeof buf, fp)) > 0)
                    out->write({buf, nr});
            }
            fclose(fp);
        }
    }
    deferred.clear();
}

void Engine::close_wfiles() {
    for (auto& [_, fp] : wfiles) if (fp) fclose(fp);
    wfiles.clear();
}

// ── Core execution loop ───────────────────────────────────────
FC Engine::run(LineSource& src) {
    const auto& P = *prog;
    int  pc       = 0;
    const int N   = static_cast<int>(P.size());

    while (pc < N) {
        const FlatCmd& c = P[pc];

        if (c.op == '}' || c.op == ':') { ++pc; continue; }

        bool active = matches(c);

        if (c.op == '{') {
            if (!active) pc = c.jmp; else ++pc;
            continue;
        }
        if (!active) { ++pc; continue; }

        // Update last_re from any address RE just tested
        if (c.a[0].k == AK::RE && c.a[0].re && c.a[0].re->ok) last_re = c.a[0].re;
        if (c.a[1].k == AK::RE && c.a[1].re && c.a[1].re->ok) last_re = c.a[1].re;

        switch (c.op) {

        // ── a: queue text for output after cycle ──────────────
        case 'a':
            deferred.push_back({false, false, c.text});
            break;

        // ── b: unconditional branch ───────────────────────────
        case 'b':
            pc = c.jmp; continue;

        // ── c: change (replace) pattern space ─────────────────
        //  With a 2-address range: print text only on last line.
        case 'c':
            if (range_ending(c)) out->writeln(c.text);
            ps.clear();
            return FC::NewCycle;

        // ── d: delete, new cycle ──────────────────────────────
        case 'd':
            ps.clear();
            return FC::NewCycle;

        // ── D: delete first line of PS, restart script ────────
        case 'D': {
            auto nl = ps.find('\n');
            if (nl == string::npos) { ps.clear(); return FC::NewCycle; }
            ps.erase(0, nl + 1);
            sub_made = false;
            pc = 0; continue;
        }

        // ── e: execute pattern space (or arg) as shell command ─
        case 'e':
            if (sandbox) die("'e' command disabled in sandbox mode");
            if (c.text.empty()) ps = exec_shell(ps);
            else out->writeln(exec_shell(c.text));
            break;

        // ── F: print filename ─────────────────────────────────
        case 'F':
            out->writeln(fname);
            break;

        // ── g / G: get hold space ─────────────────────────────
        case 'g': ps = hs;                 break;
        case 'G': ps += '\n'; ps += hs;    break;

        // ── h / H: copy to hold space ─────────────────────────
        case 'h': hs = ps;                 break;
        case 'H': hs += '\n'; hs += ps;    break;

        // ── i: print text immediately ─────────────────────────
        case 'i':
            out->writeln(c.text);
            break;

        // ── l: list ───────────────────────────────────────────
        case 'l':
            do_list(ps, c.lwidth, *out);
            break;

        // ── n: default output, read next line ─────────────────
        case 'n':
            if (!suppress) out->writeln(ps);
            flush_deferred();
            if (!src.has_more()) return FC::QuitSilent;
            { bool il; src.read(ps, fname, il); last_line = il; }
            if (separate && fname != prev_fname) { lineno = 1; prev_fname = fname; }
            else ++lineno;
            sub_made = false;
            break;

        // ── N: append next line to PS ─────────────────────────
        case 'N':
            if (!src.has_more()) {
                if (!suppress) out->writeln(ps);
                flush_deferred();
                return FC::QuitSilent;
            }
            { bool il; string nf, nl;
              src.read(nl, nf, il); last_line = il;
              if (separate && nf != prev_fname) { lineno = 1; prev_fname = nf; }
              else ++lineno;
              fname = nf;
              ps += '\n'; ps += nl;
            }
            break;

        // ── p / P: print ──────────────────────────────────────
        case 'p':
            out->writeln(ps);
            break;
        case 'P': {
            auto nl = ps.find('\n');
            out->writeln(nl == string::npos ? string_view(ps)
                                            : string_view(ps).substr(0, nl));
            break;
        }

        // ── q / Q: quit ───────────────────────────────────────
        case 'q':
            if (!suppress) out->writeln(ps);
            flush_deferred();
            g_exit_code = c.ecode;
            return FC::Quit;
        case 'Q':
            g_exit_code = c.ecode;
            return FC::Quit;

        // ── r / R: read file (deferred) ───────────────────────
        case 'r': deferred.push_back({true,  false, c.text}); break;
        case 'R': deferred.push_back({true,  true,  c.text}); break;

        // ── s: substitute ─────────────────────────────────────
        case 's':
            if (do_subst(c.s, ps, last_re)) {
                if (c.s.re && c.s.re->ok) last_re = c.s.re;
                sub_made = true;
                if (c.s.print) out->writeln(ps);
                if (!c.s.wfile.empty())       write_to_file(c.s.wfile, ps);
                if (c.s.exec) {
                    if (sandbox) die("s///e disabled in sandbox mode");
                    ps = exec_shell(ps);
                }
            }
            break;

        // ── t / T: branch on substitution flag ────────────────
        case 't': {
            bool b = sub_made; sub_made = false;
            if (b) { pc = c.jmp; continue; }
            break;
        }
        case 'T': {
            bool b = sub_made; sub_made = false;
            if (!b) { pc = c.jmp; continue; }
            break;
        }

        // ── v: version check (always pass) ────────────────────
        case 'v': break;

        // ── w / W: write to file ──────────────────────────────
        case 'w': write_to_file(c.text, ps); break;
        case 'W': {
            auto nl = ps.find('\n');
            write_to_file(c.text,
                nl == string::npos ? string_view(ps)
                                   : string_view(ps).substr(0, nl));
            break;
        }

        // ── x: exchange PS and HS ─────────────────────────────
        case 'x': std::swap(ps, hs); break;

        // ── y: transliterate ──────────────────────────────────
        case 'y': do_trans(c.ytbl, ps); break;

        // ── z: zap PS ─────────────────────────────────────────
        case 'z': ps.clear(); break;

        // ── =: print line number ──────────────────────────────
        case '=':
            out->write_long(lineno);
            out->put('\n');
            break;

        default: break;
        }
        ++pc;
    }
    return FC::Normal;
}

// ── process: outer read → run → output loop ──────────────────
void Engine::process(LineSource& src) {
    prev_fname.clear();

    while (src.read(ps, fname, last_line)) {
        if (separate && fname != prev_fname) {
            lineno = 0;
            prev_fname = fname;
            range_at.assign(total_cmds, -1L); // reset per-file
        }
        ++lineno;
        sub_made = false;

        FC fc = run(src);

        switch (fc) {
        case FC::Normal:
            if (!suppress) out->writeln(ps);
            flush_deferred();
            break;
        case FC::NewCycle:
            flush_deferred();
            break;
        case FC::Quit:
        case FC::QuitSilent:
            close_wfiles();
            return;
        }
    }
    close_wfiles();
}

} // namespace fastsed

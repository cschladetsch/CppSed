// ============================================================
//  Replacement.cpp  —  s/// replacement token parser + applicator
// ============================================================

#include "fastsed/Replacement.hpp"

namespace fastsed {

ReplVec parse_repl(string_view sv) {
  ReplVec rv;
  string acc;

  auto flush = [&] {
    if (!acc.empty()) {
      rv.push_back({RTK::Lit, acc});
      acc.clear();
    }
  };

  for (size_t i = 0; i < sv.size();) {
    char c = sv[i++];
    if (c == '\\' && i < sv.size()) {
      char nc = sv[i++];
      switch (nc) {
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        flush();
        rv.push_back({RTK::Ref, {}, nc - '0'});
        break;
      case 'n':
        acc += '\n';
        break;
      case '&':
        acc += '&';
        break;
      case 'l':
        flush();
        rv.push_back({RTK::LowerC, {}, 0});
        break;
      case 'u':
        flush();
        rv.push_back({RTK::UpperC, {}, 0});
        break;
      case 'L':
        flush();
        rv.push_back({RTK::LowerOn, {}, 0});
        break;
      case 'U':
        flush();
        rv.push_back({RTK::UpperOn, {}, 0});
        break;
      case 'E':
        flush();
        rv.push_back({RTK::CaseOff, {}, 0});
        break;
      default:
        acc += nc;
        break;
      }
    } else if (c == '&') {
      flush();
      rv.push_back({RTK::All, {}, 0});
    } else {
      acc += c;
    }
  }
  flush();
  return rv;
}

string apply_repl(const ReplVec &rv, const char *src, const regmatch_t *pm,
                  int npm) {
  string out;
  bool lo_next = false, up_next = false, lo_on = false, up_on = false;

  auto xfm = [&](char c) -> char {
    if (lo_next) {
      lo_next = false;
      return static_cast<char>(tolower((unsigned char)c));
    }
    if (up_next) {
      up_next = false;
      return static_cast<char>(toupper((unsigned char)c));
    }
    if (lo_on)
      return static_cast<char>(tolower((unsigned char)c));
    if (up_on)
      return static_cast<char>(toupper((unsigned char)c));
    return c;
  };
  auto push_sv = [&](string_view s) {
    for (char c : s)
      out += xfm(c);
  };

  for (const RTok &t : rv) {
    switch (t.k) {
    case RTK::Lit:
      push_sv(t.lit);
      break;
    case RTK::All:
      if (pm[0].rm_so >= 0)
        push_sv({src + pm[0].rm_so,
                 static_cast<size_t>(pm[0].rm_eo - pm[0].rm_so)});
      break;
    case RTK::Ref: {
      int r = t.ref;
      if (r < npm && pm[r].rm_so >= 0)
        push_sv({src + pm[r].rm_so,
                 static_cast<size_t>(pm[r].rm_eo - pm[r].rm_so)});
      break;
    }
    case RTK::NL:
      out += xfm('\n');
      break;
    case RTK::LowerC:
      lo_next = true;
      up_next = false;
      break;
    case RTK::UpperC:
      up_next = true;
      lo_next = false;
      break;
    case RTK::LowerOn:
      lo_on = true;
      up_on = false;
      break;
    case RTK::UpperOn:
      up_on = true;
      lo_on = false;
      break;
    case RTK::CaseOff:
      lo_on = up_on = lo_next = up_next = false;
      break;
    }
  }
  return out;
}

} // namespace fastsed

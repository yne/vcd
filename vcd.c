#include <ctype.h>
#include <inttypes.h> // for scanf(u64) portability
#include <stdint.h>   // u64 typedef
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "klib/khash.h" // for Channel mapping
#include "klib/kvec.h" // to keep Channel ordered in hash

#define USAGE "USAGE: vcd < in.vcd > out.ascii :\n"
#define PROLOG "Fatal error. Send the VCD on https://github.com/yne/vcd/issues"
#define REBUILD(D) #D " reached (" VAL(D) "), rebuild with -D" #D "=...\n"
#define die(...) exit(fprintf(stderr, PROLOG "\nReason: " __VA_ARGS__))

#define SFL 127 // scanf Token limit
#ifndef MAX_SCOPE
#define MAX_SCOPE 32 // how many char scopes[] to allocate
#endif
#ifndef MAX_CHANNEL
#define MAX_CHANNEL 400 // how many Channel to allocate 96*96 = 8836
#endif
#define ITV_TIME 10                // sample interval to display timestamp
#define VALUES "0123456789zZxXbU-" // allowed bus values/types
#define COUNT(A) (sizeof(A) / sizeof(*A))
#define MAX(A, B) (A > B ? A : B)
#define VAL(A) #A
#define TXT(A) VAL(A)

typedef char Token[SFL + 1]; // parsing token (channel name, scope name, ...)
typedef struct {
  char *low, *raise, *high, *drown, *start, *end;
  unsigned skip;
} PrintOpt;
typedef struct {
  // could have as much state as it bus size but nobody handle such case
  // 'UZX' => show state
  // '\0' => show .val
  char state;
  unsigned val;
} Sample;
typedef struct {
  unsigned size;
  unsigned scope;
  Token name;
  Sample *samples;
} Channel;
KHASH_MAP_INIT_INT(Channel, Channel) // KHASH_MAP_INIT_STR fail on same pointer

typedef struct {
  khash_t(Channel) * ch;   // [0] = timestamps
  kvec_t(char*) keys;
  Token scopes[MAX_SCOPE]; // [0] = default
  unsigned total, scope_count;
  float scale;               // duration of each sample
  Token date, version, unit; // file info
  // parsing related values
  unsigned scope_cur;
  unsigned scope_lim, ch_lim, sz_lim;
} ParseCtx;

size_t unilen(char *s) {
  size_t j = 0;
  for (; *s; s++)
    j += ((*s & 0xc0) != 0x80);
  return j;
}

/* read a $instruction and it opt if needed until $end*/
void parseVcdInstruction(ParseCtx *p) {
  Token token;
  scanf("%" TXT(SFL) "s", token);
  if (!strcmp("var", token)) {
    Token id;
    Channel c = {.scope = p->scope_cur, .samples = malloc(sizeof(Sample))};
    scanf(" %*s %u %" TXT(SFL) "[^ ] %" TXT(SFL) "[^$]", &c.size, id, c.name);
    p->ch_lim = MAX(p->ch_lim, strlen(c.name));
    p->sz_lim = MAX(p->sz_lim, c.size);
    int ret;
    khiter_t k = kh_put(Channel, p->ch, kh_str_hash_func(id), &ret);
    kv_push(char*, p->keys, strdup(id)); // append
    if (ret == 0)
      fprintf(stderr, "WARN: %s already exist\n", id);
    kh_value(p->ch, k) = c;
  } else if (!strcmp("scope", token)) {
    p->scope_count++;
    if (p->scope_count == MAX_SCOPE)
      die(REBUILD(MAX_SCOPE));
    p->scope_cur = p->scope_count;
    scanf("%*s %" TXT(SFL) "[^ $]", p->scopes[p->scope_cur]);
    p->scope_lim = MAX(p->scope_lim, strlen(p->scopes[p->scope_cur]));
  } else if (!strcmp("date", token)) {
    scanf("\n%" TXT(SFL) "[^$\n]", p->date);
  } else if (!strcmp("version", token)) {
    scanf("\n%" TXT(SFL) "[^$\n]", p->version);
  } else if (!strcmp("timescale", token)) {
    scanf("\n%f%" TXT(SFL) "[^$\n]", &p->scale, p->unit);
  } else if (!strcmp("comment", token)) {
    scanf("\n%*[^$]");
  } else if (!strcmp("upscope", token)) {
    scanf("\n%*[^$]");
    p->scope_cur = 0; // back to the root
  } else if (!strcmp("enddefinitions", token)) {
    scanf("\n%*[^$]");
  } else if (!strcmp("dumpvars", token)) {
  } else if (!strcmp("end", token)) {
  } else {
    printf("unknown token : %s\n", token);
  }
}
/* Parse a time line (ex: '#210000000') and copy all previous samples values */
void parseVcdTimestamp(ParseCtx *p) {
  // copy previous sample on every channel
  if (p->total > 0) {
    for (khiter_t k = kh_begin(p->ch); k != kh_end(p->ch); ++k) {
      if (!kh_exist(p->ch, k))
        continue;
      Channel *ch = &kh_value(p->ch, k);
      if (!ch->size)
        continue;
      ch->samples = realloc(ch->samples, sizeof(Sample) * (p->total + 1));
      ch->samples[p->total] = ch->samples[p->total - 1];
    }
  }
  uint64_t _unused;
  scanf("%" PRIu64, &_unused); // p->timestamps[p->total]
  p->total++;
}
/*
sample line end with the channel ID and start either with a state or data:
1^
Z^
b0100 ^
0! 0" 1# 0$ 1% 0& 1'
*/
void parseVcdSample(ParseCtx *p, int c) {
  Sample s = {'\0', 0};
  if (c == 'b') {
    for (c = getchar(); c != EOF && c != ' '; c = getchar()) {
      if (c == '0' || c == '1') {
        s.val = s.val * 2 + (c - '0');
      } else if (strchr(VALUES, c)) {
        s.state = c;
      } else {
        die("Unknown sample value: %c", c);
      }
    }
  } else {
    s.state = isalpha(c) ? c : '\0';
    s.val = isdigit(c) ? c - '0' : 0;
  }
  Token id_str;
  scanf("%" TXT(SFL) "[^ \n]", id_str);
  if (!p->total)
    return; // ROHD define value BEFORE timestamp #0
  khiter_t k = kh_get(Channel, p->ch, kh_str_hash_func(id_str));
  if ((k == kh_end(p->ch))) {
    printf("%s don't exist %i ?\n", id_str, k);
  }
  kh_value(p->ch, k).samples[p->total - 1] = s;
}

void parseVcd(ParseCtx *p) {
  for (int c = getchar(); c != EOF; c = getchar()) {
    if (isspace(c))
      continue;
    if (c == '$') {
      parseVcdInstruction(p);
    } else if (c == '#') {
      parseVcdTimestamp(p);
    } else if (strchr(VALUES, c)) {
      parseVcdSample(p, c);
    } else {
      die("unknow char : %c\n", c);
    }
  }
}

void printYml(ParseCtx *p, PrintOpt *opt) {
  if (unilen(opt->high) != 1)
    die("high waveform length must be 1");
  if (unilen(opt->low) != 1)
    die("low waveform length must be 1");
  if (unilen(opt->drown) > 1)
    die("drown waveform length must be 1 or empty");
  if (unilen(opt->raise) > 1)
    die("raise waveform length must be 1 or empty");

  int zoom = (p->sz_lim + 7) >> 2; // how many char per sample (8bit => 2)
  int trans = *opt->drown && *opt->raise;
  printf("global:\n");
  printf("  zoom: %i\n", zoom);
  printf("  date: %s\n", p->date);
  printf("  total: %i\n", p->total);
  printf("  skip: %i\n", opt->skip);
  printf("  time:\n");
  printf("    scale: %.2f\n", p->scale);
  printf("    unit: %s\n", p->unit ?: "?");
  printf("    %-*s: %s", p->ch_lim, "line", opt->start);
  for (double smpl = opt->skip; smpl < p->total; smpl += ITV_TIME) {
    printf("%-*g ", ITV_TIME * zoom - 1, smpl * p->scale);
  }
  printf("%s\nchannels:\n", opt->end);
  Channel *prev_ch = NULL;
  for (size_t i = 0; i < kv_size(p->keys); i++) {
    khiter_t k = kh_get(Channel, p->ch, kh_str_hash_func(kv_A(p->keys,i)));
    if (!kh_exist(p->ch, k))
      continue;
    Channel *ch = &kh_value(p->ch, k);
    // skip empty ch
    if (!ch->size)
      continue;
    // print scope (if changed)
    if (!prev_ch || ch->scope != (prev_ch->scope)) {
      printf("  %s:\n", ch->scope ? p->scopes[ch->scope] : "default");
    }

    printf("    %-*s: %s", p->ch_lim, ch->name, opt->start);
    for (Sample *s = ch->samples + opt->skip; s < ch->samples + p->total; s++) {
      Sample *prev = s > ch->samples ? s - 1 : s;
      if (s->state) { // state data: UUUUZZZZ-
        printf("%-*c", zoom, s->state);
      } else if (ch->size == 1) { // binary wave: ▁▁/▔▔
        // have a different data => print a transition
        for (int w = 0; w < zoom; w++) {
          if (!w && trans && s->state == prev->state && s->val != prev->val)
            printf("%s", prev->val ? opt->drown : opt->raise);
          else
            printf("%s", s->val ? opt->high : opt->low);
        }
      } else { // bus : show hex value
        printf("%-*X", zoom, s->val);
      }
    }
    printf("%s\n", opt->end);
    prev_ch = ch;
  }
}

int main() {
  PrintOpt opt = {getenv("LOW") ?: "▁",       getenv("RAISE") ?: "╱",
                  getenv("HIGH") ?: "▔",      getenv("DROWN") ?: "╲",
                  getenv("STX") ?: "\"",      getenv("ETX") ?: "\"",
                  atoi(getenv("SKIP") ?: "0")};
  // PrintOpt opt = {"_", "/", "#", "\\"} {"▁", "╱", "▔", "╲"};
  ParseCtx ctx = {.ch = kh_init(Channel)};
  parseVcd(&ctx);
  printYml(&ctx, &opt);
}

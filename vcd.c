#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SAMPLE 2048
#define MAX_CHANNEL 400
#define MAX_NAME 128
#define COUNT(A) (sizeof(A) / sizeof((A)[0]))
#define MAX(A, B) (A > B ? A : B)
#define VAL(A) #A
#define TXT(A) VAL(A)

typedef struct {
  int width;
  int verbose;
  int round;
  int colsize;
  int timescalestep;
  char* scope;
  FILE* fin;
  FILE* fout;
} Parameters;

typedef struct {
  int size;
  int scope;
  char name[MAX_NAME];
  char type[MAX_SAMPLE];  //'U','Z','X','\0'=Data
  unsigned val[MAX_SAMPLE];
} Channel;

typedef struct {
  long long unsigned timestamps[MAX_SAMPLE];
  Channel ch[MAX_CHANNEL];
  char scopes[32][32];  //[0]=root
  int nb, nb_scopes;
  char date[32], version[32], scale[32];  // file info
  // parsing related values
  int cur_scopes;
} Parser;

void showHelp(char* arg0, Parameters* p) {
  fprintf(
      stderr,
      "Usage: %s [OPTION] [FILE]...:\n"
      " -h:			: this help screen\n"
      " -v=%i			: verbosity "
      "(0:fatal,1:error,2:warning,3:debug)\n"
      " -w=%i			: width of each sample  (1,2,...)\n"
      " -r=%i			: rounded wave (0:none,1:pipe,2:slash)\n"
      " -c=%i			: column width\n"
      " -t=%i			: time scale step (0:none,1,10,...)\n"
      " -s=a,b,c		: comma separated scope(s) to display\n",
      arg0, p->verbose, p->width, p->round, p->colsize, p->timescalestep);
}

void parseArgs(int argc, char** argv, Parameters* params) {
  int i;
  for (i = 1; i < argc; i++) {  // parse "-" arguments
    if (argv[i][0] != '-') {
      params->fin = fopen(argv[i], "r");
      continue;
    }
    switch (argv[i][1]) {
      case 'o':
        params->fout = fopen(argv[i] + 3, "w+");
        break;
      case 'h':
        showHelp(argv[0], params);
        exit(0);
        break;
      case 'w':
        params->width = MAX(1, atoi(argv[i] + 3));
        break;
      case 'r':
        params->round = atoi(argv[i] + 3);
        break;
      case 'c':
        params->colsize = atoi(argv[i] + 3);
        break;
      case 't':
        params->timescalestep = atoi(argv[i] + 3);
        break;
      case 's':
        params->scope = argv[i] + 3;
        break;
      default:
        fprintf(stderr, "unknow param '%c'", argv[i][1]);
    }
  }
}

int char2id(char* str_id) {
  int i = strlen(str_id) - 1, id = 0;
  for (; i >= 0; i--) {
    id *= 94;               // shift previous value
    id += str_id[i] - '!';  //! is 0, ~ is 93
  }
  return id;
}

void parseInst(Parameters* params, Parser* p) {
  char token[32];
  fscanf(params->fin, "%31s", token);
  // printf("%s\n",token);
  if (!strcmp("var", token)) {
    char id_str[4];
    Channel chan = {};
    fscanf(params->fin, " reg %d %3[^ ] %" TXT(MAX_NAME) "[^$]", &(chan.size),
           id_str, chan.name);
    fscanf(params->fin, " wire %d %3[^ ] %" TXT(MAX_NAME) "[^$]", &(chan.size),
           id_str, chan.name);
    int id = char2id(id_str);
    p->ch[id] = chan;  // printf("size=%i <%c> name=<%s>\n",size,id,data);
    p->ch[id].scope = p->cur_scopes;
  } else if (!strcmp("scope", token)) {
    fscanf(params->fin, "%*127s %127[^ $]",
           p->scopes[p->cur_scopes = ++(p->nb_scopes)]);
  } else if (!strcmp("date", token)) {
    fscanf(params->fin, "\n%31[^$\n]", p->date);
  } else if (!strcmp("version", token)) {
    fscanf(params->fin, "\n%31[^$\n]", p->version);
  } else if (!strcmp("timescale", token)) {
    fscanf(params->fin, "\n%127[^$\n]", p->scale);
  } else if (!strcmp("comment", token)) {
    fscanf(params->fin, "\n%*[^$]");
  } else if (!strcmp("upscope", token)) {
    fscanf(params->fin, "\n%*[^$]");
    p->cur_scopes = 0;
  } /*back to root */
  else if (!strcmp("enddefinitions", token)) {
    fscanf(params->fin, "\n%*[^$]");
  } else if (!strcmp("dumpvars", token)) {
  } else if (!strcmp("end", token)) {
  } else {
    printf("unknow token : %s\n", token);
  }
}

void parseTime(Parameters* params, Parser* p) {
  long long unsigned stamp = 0;
  int i, c;
  while ((c = fgetc(params->fin)) != EOF) {
    if (isdigit(c)) {
      stamp = stamp * 10 + (c - '0');
    } else {
      ungetc(c, params->fin);
      if (p->nb >= COUNT(p->timestamps)) return;
      if (p->nb > 0) {  // copy all previous channels_size val to the current
                        // one
        for (i = 0; i < COUNT(p->ch); i++) {
          p->ch[i].val[p->nb] = p->ch[i].val[p->nb - 1];
          p->ch[i].type[p->nb] = p->ch[i].type[p->nb - 1];
        }
      }
      p->timestamps[p->nb] = stamp;
      p->nb++;
      return;
    }
  }
}
/*
there is 2 kinds of data definition line :
state    : 1^
bus data : b0100001001011001110 ^
*/
void parseData(Parameters* params, Parser* p) {
  unsigned data = 0;
  char id_str[4], type = '\0';
  int id = 0, c = fgetc(params->fin);
  if (c == 'b') {  // parsing bus data b%[0-9UZX]+ %c
    while ((c = fgetc(params->fin)) != EOF && c != ' ') {
      if (isdigit(c))
        data = data * 2 + (c - '0');
      else {
        // letter (Z,U,X,...) = undefined type, but we don't know the id yet
        if (c == 'x' || c == 'z')
          type = toupper(c);
        else
          type = c;
      }
    }
    fscanf(params->fin, "%3[^\n]", id_str);
    id = char2id(id_str);
    p->ch[id].type[p->nb - 1] = type;
    p->ch[id].val[p->nb - 1] = data;
  } else {  // parsing state %[0-9UZX] %c
    fscanf(params->fin, "%3[^\n]", id_str);
    id = char2id(id_str);
    if (isalpha(c))
      p->ch[id].type[p->nb - 1] = c;
    else
      p->ch[id].type[p->nb - 1] = '\0';  // letter (Z,U,X,...) = undefined type
    if (isdigit(c)) p->ch[id].val[p->nb - 1] = c - '0';
  }
}

void parseFile(Parameters* params, Parser* p) {
  int c;
  while ((c = fgetc(params->fin)) != EOF) {
    //		printf("?%c\n",c);
    if (isspace(c)) continue;
    if (c == '$') {
      parseInst(params, p);
    } else if (isdigit(c) || c == 'b' || c == 'Z' || c == 'U' || c == 'x' ||
               c == 'z') {
      if (c == 'x' || c == 'z')
        ungetc(toupper(c), params->fin);
      else
        ungetc(c, params->fin);
      parseData(params, p);
    } else if (c == '#') {
      parseTime(params, p);
    } else {
      fprintf(stderr, "unknow char : %c\n", c);
    }
  }
}

unsigned numDischarges(unsigned n) {
  unsigned i = 0;
  do {
    n = n / 10;
    i += 1;
  } while (n > 0);
  return i;
}
void numDelete(char* instr, char* outstr) {
  unsigned index = 0;
  for (unsigned i = 0; i < strlen(instr); i++)
    if (instr[i] < '0' || instr[i] > '9') {
      outstr[index] = instr[i];
      ++index;
    }
  outstr[index] = '\0';
}

void showVertical(Parameters* params, Parser* p) {
  int w;
  unsigned chan, smpl;
  if (p->nb) fprintf(params->fout, "%i samples", p->nb);
  if (p->date[0]) fprintf(params->fout, " / %s", p->date);
  if (p->scale[0]) fprintf(params->fout, " / %s", p->scale);
  fprintf(params->fout, "\n");

  char scalestr[32] = {'\0'};
  numDelete(p->scale, scalestr);

  for (chan = 0; chan < COUNT(p->ch); chan++) {
    if (!p->ch[chan].size) continue;  // skip empty ch
    if (params->scope && (!p->ch[chan].scope ||
                          !strstr(params->scope, p->scopes[p->ch[chan].scope])))
      continue;  // skip root node or unrelated node if scope-only wanted
    if ((!chan && p->ch[chan].scope) ||
        (chan > 0 && (p->ch[chan].scope != p->ch[chan - 1].scope))) {
      unsigned timescalestep = params->timescalestep;
      if (timescalestep != 0) {
        fprintf(
            params->fout, "┌── %s%*.*stime: ",
            p->ch[chan].scope ? p->scopes[p->ch[chan].scope] : "",
            params->colsize -
                strlen(p->ch[chan].scope ? p->scopes[p->ch[chan].scope] : "") -
                2);
        for (smpl = 0; smpl < p->nb; smpl += timescalestep) {
          unsigned scalenum = smpl * atoi(p->scale);
          fprintf(params->fout, "▏%d%s%*.*s", scalenum, scalestr,
                  timescalestep * params->width - 1 - numDischarges(scalenum) -
                      strlen(scalestr));
        }
        fprintf(params->fout, "\n│\n");
      } else
        fprintf(params->fout, "┌── %s\n",
                p->ch[chan].scope ? p->scopes[p->ch[chan].scope] : "");
    }
    fprintf(params->fout, "%s %*.*s[%2i]: ", p->ch[chan].scope ? "│" : " ",
            params->colsize, params->colsize, p->ch[chan].name,
            p->ch[chan].size);
    for (smpl = 0; smpl < p->nb; smpl += 1) {
      char type = p->ch[chan].type[smpl];
      unsigned data = p->ch[chan].val[smpl];
      if (p->ch[chan].size == 1) {  // binary
        w = params->width;
        // have a previous data => can print a transition
        if (params->round && smpl > 0 && !p->ch[chan].type[smpl - 1]) {
          if (p->ch[chan].val[smpl] !=
              p->ch[chan].val[smpl - 1]) {  // the value changed
            // from H to L or L to H ?
            fprintf(params->fout, params->round == 2 ? "%s" : "│",
                    (p->ch[chan].val[smpl - 1] ? "╲" : "╱"));
            w -= 1;
          }
        }
        while (w-- > 0) {
          if (type)
            fprintf(params->fout, "%c", type);
          else
            fprintf(params->fout, "%s", data ? "▔" : "▁");
        }
      } else {  // bus
        int is_bin = p->ch[chan].type[smpl];
        fprintf(params->fout, is_bin ? "%*c" : "%*X", params->width,
                is_bin ? p->ch[chan].type[smpl] : p->ch[chan].val[smpl]);
      }
    }
    fprintf(params->fout, "%s%s\n", params->width >= 2 ? "\n" : " ",
            p->ch[chan].scope && params->width >= 2 ? "│" : " ");
  }
}

int main(int argc, char** argv) {
  Parameters params = {
      .verbose = 0,
      .width = 2,
      .round = 2,
      .colsize = 32,
      .timescalestep = 0,
      .fin = stdin,
      .fout = stdout,
  };
  Parser data;
  memset(&data, 0, sizeof(Parser));

  parseArgs(argc, argv, &params);
  if (!params.fin) {
    return fprintf(stderr, "no input stream\n"), -1;
  }
  parseFile(&params, &data);
  showVertical(&params, &data);
  fclose(params.fin);
  return 0;
}

#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "vterm.h"

static int parser_text(const char bytes[], size_t len, void *user)
{
  int i;
  for(i = 0; i < len; i++)
    if(bytes[i] < 0x20 || (bytes[i] >= 0x80 && bytes[i] < 0xa0))
      break;

  printf("%.*s", i, bytes);
  return i;
}

/* 0     1      2      3       4     5      6      7      8      9      A      B      C      D      E      F    */
static const char *name_c0[] = {
  "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL", "BS",  "HT",  "LF",  "VT",  "FF",  "CR",  "LS0", "LS1",
  "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB", "CAN", "EM",  "SUB", "ESC", "FS",  "GS",  "RS",  "US",
};
static const char *name_c1[] = {
  NULL,  NULL,  "BPH", "NBH", NULL,  "NEL", "SSA", "ESA", "HTS", "HTJ", "VTS", "PLD", "PLU", "RI",  "SS2", "SS3",
  "DCS", "PU1", "PU2", "STS", "CCH", "MW",  "SPA", "EPA", "SOS", NULL,  "SCI", "CSI", "ST",  "OSC", "PM",  "APC",
};

static int parser_control(unsigned char control, void *user)
{
  if(control < 0x20)
    printf("{%s}", name_c0[control]);
  else if(control >= 0x80 && control < 0xa0 && name_c1[control - 0x80])
    printf("{%s}", name_c1[control]);
  else
    printf("{CONTROL 0x%02x}", control);

  if(control == 0x0a)
    printf("\n");
  return 1;
}

static int parser_escape(const char bytes[], size_t len, void *user)
{
  if(bytes[0] >= 0x20 && bytes[0] < 0x30) {
    if(len < 2)
      return -1;
    len = 2;
  }
  else {
    len = 1;
  }

  printf("{ESC ");
  for(int i = 0; i < len; i++)
    printf("%c ", bytes[i]);
  printf("}");

  return len;
}

/* 0     1      2      3       4     5      6      7      8      9      A      B      C      D      E      F    */
static const char *name_csi_plain[] = {
  "ICH", "CUU", "CUD", "CUF", "CUB", "CNL", "CPL", "CHA", "CUP", "CHT", "ED",  "EL",  "IL",  "DL",  "EF",  "EA",
  "DCH", "SSE", "CPR", "SU",  "SD",  "NP",  "PP",  "CTC", "ECH", "CVT", "CBT", "SRS", "PTX", "SDS", "SIMD",NULL,
  "HPA", "HPR", "REP", "DA",  "VPA", "VPR", "HVP", "TBC", "SM",  "MC",  "HPB", "VPB", "RM",  "SGR", "DSR", "DAQ",
};

/*0           4           8           B         */
static const int newline_csi_plain[] = {
  0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0,
};

static int parser_csi(const char *leader, const long args[], int argcount, const char *intermed, char command, void *user)
{
  const char *name = NULL;
  if(!leader && !intermed && command < 0x70)
    name = name_csi_plain[command - 0x40];

  if(newline_csi_plain[command - 0x40])
    printf("\n");

  if(name)
    printf("{%s", name);
  else
    printf("{CSI");

  if(leader && leader[0])
    printf(" %s", leader);

  for(int i = 0; i < argcount; i++) {
    printf(i ? "," : " ");

    if(args[i] == CSI_ARG_MISSING)
      printf("*");
    while(CSI_ARG_HAS_MORE(args[i]))
      printf("%ld+", CSI_ARG(args[i++]));
    printf("%ld", CSI_ARG(args[i]));
  }

  if(intermed && intermed[0])
    printf(" %s", intermed);

  if(name)
    printf("}");
  else
    printf(" %c}", command);

  return 1;
}

static int parser_osc(const char *command, size_t cmdlen, void *user)
{
  printf("{OSC %.*s}", (int)cmdlen, command);

  return 1;
}

static int parser_dcs(const char *command, size_t cmdlen, void *user)
{
  printf("{DCS %.*s}", (int)cmdlen, command);

  return 1;
}

static VTermParserCallbacks parser_cbs = {
  .text    = &parser_text,
  .control = &parser_control,
  .escape  = &parser_escape,
  .csi     = &parser_csi,
  .osc     = &parser_osc,
  .dcs     = &parser_dcs,
};

int main(int argc, char *argv[])
{
  int fd = open(argv[1], O_RDONLY);
  if(fd == -1) {
    fprintf(stderr, "Cannot open %s - %s\n", argv[1], strerror(errno));
    exit(1);
  }

  /* Size matters not for the parser */
  VTerm *vt = vterm_new(25, 80);
  vterm_set_parser_callbacks(vt, &parser_cbs, NULL);

  int len;
  char buffer[1024];
  while((len = read(fd, buffer, sizeof(buffer))) > 0) {
    vterm_push_bytes(vt, buffer, len);
  }

  printf("\n");

  close(fd);
  vterm_free(vt);
}

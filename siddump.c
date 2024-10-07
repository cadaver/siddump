#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "cpu.h"

#define MAX_INSTR 0x100000

typedef struct
{
  unsigned short freq;
  unsigned short pulse;
  unsigned short adsr;
  unsigned char wave;
  int note;
} CHANNEL;

typedef struct
{
  unsigned short cutoff;
  unsigned char ctrl;
  unsigned char type;
} FILTER;

int main(int argc, char **argv);
unsigned char readbyte(FILE *f);
unsigned short readword(FILE *f);

CHANNEL chn[3];
CHANNEL prevchn[3];
CHANNEL prevchn2[3];
FILTER filt;
FILTER prevfilt;

extern unsigned short pc;

const char *notename[] =
 {"C-0", "C#0", "D-0", "D#0", "E-0", "F-0", "F#0", "G-0", "G#0", "A-0", "A#0", "B-0",
  "C-1", "C#1", "D-1", "D#1", "E-1", "F-1", "F#1", "G-1", "G#1", "A-1", "A#1", "B-1",
  "C-2", "C#2", "D-2", "D#2", "E-2", "F-2", "F#2", "G-2", "G#2", "A-2", "A#2", "B-2",
  "C-3", "C#3", "D-3", "D#3", "E-3", "F-3", "F#3", "G-3", "G#3", "A-3", "A#3", "B-3",
  "C-4", "C#4", "D-4", "D#4", "E-4", "F-4", "F#4", "G-4", "G#4", "A-4", "A#4", "B-4",
  "C-5", "C#5", "D-5", "D#5", "E-5", "F-5", "F#5", "G-5", "G#5", "A-5", "A#5", "B-5",
  "C-6", "C#6", "D-6", "D#6", "E-6", "F-6", "F#6", "G-6", "G#6", "A-6", "A#6", "B-6",
  "C-7", "C#7", "D-7", "D#7", "E-7", "F-7", "F#7", "G-7", "G#7", "A-7", "A#7", "B-7"};

const char *filtername[] =
 {"Off", "Low", "Bnd", "L+B", "Hi ", "L+H", "B+H", "LBH"};

unsigned char freqtbllo[] = {
  0x17,0x27,0x39,0x4b,0x5f,0x74,0x8a,0xa1,0xba,0xd4,0xf0,0x0e,
  0x2d,0x4e,0x71,0x96,0xbe,0xe8,0x14,0x43,0x74,0xa9,0xe1,0x1c,
  0x5a,0x9c,0xe2,0x2d,0x7c,0xcf,0x28,0x85,0xe8,0x52,0xc1,0x37,
  0xb4,0x39,0xc5,0x5a,0xf7,0x9e,0x4f,0x0a,0xd1,0xa3,0x82,0x6e,
  0x68,0x71,0x8a,0xb3,0xee,0x3c,0x9e,0x15,0xa2,0x46,0x04,0xdc,
  0xd0,0xe2,0x14,0x67,0xdd,0x79,0x3c,0x29,0x44,0x8d,0x08,0xb8,
  0xa1,0xc5,0x28,0xcd,0xba,0xf1,0x78,0x53,0x87,0x1a,0x10,0x71,
  0x42,0x89,0x4f,0x9b,0x74,0xe2,0xf0,0xa6,0x0e,0x33,0x20,0xff};

unsigned char freqtblhi[] = {
  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x02,
  0x02,0x02,0x02,0x02,0x02,0x02,0x03,0x03,0x03,0x03,0x03,0x04,
  0x04,0x04,0x04,0x05,0x05,0x05,0x06,0x06,0x06,0x07,0x07,0x08,
  0x08,0x09,0x09,0x0a,0x0a,0x0b,0x0c,0x0d,0x0d,0x0e,0x0f,0x10,
  0x11,0x12,0x13,0x14,0x15,0x17,0x18,0x1a,0x1b,0x1d,0x1f,0x20,
  0x22,0x24,0x27,0x29,0x2b,0x2e,0x31,0x34,0x37,0x3a,0x3e,0x41,
  0x45,0x49,0x4e,0x52,0x57,0x5c,0x62,0x68,0x6e,0x75,0x7c,0x83,
  0x8b,0x93,0x9c,0xa5,0xaf,0xb9,0xc4,0xd0,0xdd,0xea,0xf8,0xff};

int main(int argc, char **argv)
{
  int subtune = 0;
  int seconds = 60;
  int instr = 0;
  int frames = 0;
  int spacing[2];
  int pattspacing = 0;
  int firstframe = 0;
  int counter = 0;
  int basefreq = 0;
  int basenote = 0xb0;
  int lowres = 0;
  int rows = 0;
  int oldnotefactor = 1;
  int timeseconds = 0;
  int usage = 0;
  int profiling = 0;
  int tempoindex = 0;
  unsigned loadend;
  unsigned loadpos;
  unsigned loadsize;
  unsigned loadaddress;
  unsigned initaddress;
  unsigned playaddress;
  unsigned dataoffset;
  FILE *in;
  char *sidname = 0;
  int c;

  // Scan arguments
  for (c = 1; c < argc; c++)
  {
    if (argv[c][0] == '-')
    {
      switch(toupper(argv[c][1]))
      {
        case '?':
        usage = 1;
        break;

        case 'A':
        sscanf(&argv[c][2], "%u", &subtune);
        break;

        case 'C':
        sscanf(&argv[c][2], "%x", &basefreq);
        break;

        case 'D':
        sscanf(&argv[c][2], "%x", &basenote);
        break;

        case 'F':
        sscanf(&argv[c][2], "%u", &firstframe);
        break;

        case 'L':
        lowres = 1;
        break;

        case 'N':
        // Funktempo
        if (strchr(argv[c], ','))
            sscanf(&argv[c][2], "%u,%u", &spacing[0], &spacing[1]);
        else
        {
            sscanf(&argv[c][2], "%u", &spacing[0]);
            spacing[1] = spacing[0];
        }
        break;

        case 'O':
        sscanf(&argv[c][2], "%u", &oldnotefactor);
        if (oldnotefactor < 1) oldnotefactor = 1;
        break;

        case 'P':
        sscanf(&argv[c][2], "%u", &pattspacing);
        break;

        case 'S':
        timeseconds = 1;
        break;

        case 'T':
        sscanf(&argv[c][2], "%u", &seconds);
        break;
        
        case 'Z':
        profiling = 1;
        break;
      }
    }
    else 
    {
      if (!sidname) sidname = argv[c];
    }
  }

  // Usage display
  if ((argc < 2) || (usage))
  {
    printf("Usage: SIDDUMP <sidfile> [options]\n"
           "Warning: CPU emulation may be buggy/inaccurate, illegals support very limited\n\n"
           "Options:\n"
           "-a<value> Accumulator value on init (subtune number) default = 0\n"
           "-c<value> Frequency recalibration. Give note frequency in hex\n"
           "-d<value> Select calibration note (abs.notation 80-DF). Default middle-C (B0)\n"
           "-f<value> First frame to display, default 0\n"
           "-l        Low-resolution mode (only display 1 row per note)\n"
           "-n<value> Note spacing, default 0 (none). Use <value>,<value> to specify a funktempo\n"
           "-o<value> ""Oldnote-sticky"" factor. Default 1, increase for better vibrato display\n"
           "          (when increased, requires well calibrated frequencies)\n"
           "-p<value> Pattern spacing, default 0 (none)\n"
           "-s        Display time in minutes:seconds:frame format\n"
           "-t<value> Playback time in seconds, default 60\n"
           "-z        Include CPU cycles+rastertime (PAL)+rastertime, badline corrected\n");
    return 1;
  }

  // Recalibrate frequencytable
  if (basefreq)
  {
    basenote &= 0x7f;
    if ((basenote < 0) || (basenote > 96))
    {
      printf("Warning: Calibration note out of range. Aborting recalibration.\n");
    }
    else
    {
      for (c = 0; c < 96; c++)
      {
        double note = c - basenote;
        double freq = (double)basefreq * pow(2.0, note/12.0);
        int f = freq;
        if (freq > 0xffff) freq = 0xffff;
        freqtbllo[c] = f & 0xff;
        freqtblhi[c] = f >> 8;
      }
    }
  }

  // Check other parameters for correctness
  if ((lowres) && (!spacing[0])) lowres = 0;

  // Open SID file
  if (!sidname)
  {
    printf("Error: no SID file specified.\n");
    return 1;
  }

  in = fopen(sidname, "rb");
  if (!in)
  {
    printf("Error: couldn't open SID file.\n");
    return 1;
  }

  // Read interesting parts of the SID header
  fseek(in, 6, SEEK_SET);
  dataoffset = readword(in);
  loadaddress = readword(in);
  initaddress = readword(in);
  playaddress = readword(in);
  fseek(in, dataoffset, SEEK_SET);
  if (loadaddress == 0)
    loadaddress = readbyte(in) | (readbyte(in) << 8);

  // Load the C64 data
  loadpos = ftell(in);
  fseek(in, 0, SEEK_END);
  loadend = ftell(in);
  fseek(in, loadpos, SEEK_SET);
  loadsize = loadend - loadpos;
  if (loadsize + loadaddress >= 0x10000)
  {
    printf("Error: SID data continues past end of C64 memory.\n");
    fclose(in);
    return 1;
  }
  fread(&mem[loadaddress], loadsize, 1, in);
  fclose(in);

  // Print info & run initroutine
  printf("Load address: $%04X Init address: $%04X Play address: $%04X\n", loadaddress, initaddress, playaddress);
  printf("Calling initroutine with subtune %d\n", subtune);
  mem[0x01] = 0x37;
  initcpu(initaddress, subtune, 0, 0);
  instr = 0;
  while (runcpu())
  {
    // Allow SID model detection (including $d011 wait) to eventually terminate
    ++mem[0xd012];
    if (!mem[0xd012] || ((mem[0xd011] & 0x80) && mem[0xd012] >= 0x38))
    {
        mem[0xd011] ^= 0x80;
        mem[0xd012] = 0x00;
    }
    instr++;
    if (instr > MAX_INSTR)
    {
      printf("Warning: CPU executed a high number of instructions in init, breaking\n");
      break;
    }
  }

  if (playaddress == 0)
  {
    printf("Warning: SID has play address 0, reading from interrupt vector instead\n");
    if ((mem[0x01] & 0x07) == 0x5)
      playaddress = mem[0xfffe] | (mem[0xffff] << 8);
    else
      playaddress = mem[0x314] | (mem[0x315] << 8);
    printf("New play address is $%04X\n", playaddress);
  }

  // Clear channelstructures in preparation & print first time info
  memset(&chn, 0, sizeof chn);
  memset(&filt, 0, sizeof filt);
  memset(&prevchn, 0, sizeof prevchn);
  memset(&prevchn2, 0, sizeof prevchn2);
  memset(&prevfilt, 0, sizeof prevfilt);
  printf("Calling playroutine for %d frames, starting from frame %d\n", seconds*50, firstframe);
  printf("Middle C frequency is $%04X\n\n", freqtbllo[48] | (freqtblhi[48] << 8));
  printf("| Frame | Freq Note/Abs WF ADSR Pul | Freq Note/Abs WF ADSR Pul | Freq Note/Abs WF ADSR Pul | FCut RC Typ V |");
  if (profiling)
  { // CPU cycles, Raster lines, Raster lines with badlines on every 8th line, first line included
    printf(" Cycl RL RB |");
  }
  printf("\n");
  printf("+-------+---------------------------+---------------------------+---------------------------+---------------+");
  if (profiling)
  {
    printf("------------+");
  }
  printf("\n");

  // Data collection & display loop
  while (frames < firstframe + seconds*50)
  {
    int c;

    // Run the playroutine
    instr = 0;
    initcpu(playaddress, 0, 0, 0);
    while (runcpu())
    {
      instr++;
      if (instr > MAX_INSTR)
      {
        printf("Error: CPU executed abnormally high amount of instructions in playroutine, exiting\n");
        return 1;
      }
      // Test for jump into Kernal interrupt handler exit
      if ((mem[0x01] & 0x07) != 0x5 && (pc == 0xea31 || pc == 0xea81))
        break;
    }

    // Get SID parameters from each channel and the filter
    for (c = 0; c < 3; c++)
    {
      chn[c].freq = mem[0xd400 + 7*c] | (mem[0xd401 + 7*c] << 8);
      chn[c].pulse = (mem[0xd402 + 7*c] | (mem[0xd403 + 7*c] << 8)) & 0xfff;
      chn[c].wave = mem[0xd404 + 7*c];
      chn[c].adsr = mem[0xd406 + 7*c] | (mem[0xd405 + 7*c] << 8);
    }
    filt.cutoff = (mem[0xd415] << 5) | (mem[0xd416] << 8);
    filt.ctrl = mem[0xd417];
    filt.type = mem[0xd418];

    // Frame display
    if (frames >= firstframe)
    {
      char output[512];
      int time = frames - firstframe;
      output[0] = 0;      

      if (!timeseconds)
        sprintf(&output[strlen(output)], "| %5d | ", time);
      else
        sprintf(&output[strlen(output)], "|%01d:%02d.%02d| ", time/3000, (time/50)%60, time%50);

      // Loop for each channel
      for (c = 0; c < 3; c++)
      {
        int newnote = 0;

        // Keyoff-keyon sequence detection
        if (chn[c].wave >= 0x10)
        {
          if ((chn[c].wave & 1) && ((!(prevchn2[c].wave & 1)) || (prevchn2[c].wave < 0x10)))
            prevchn[c].note = -1;
        }

        // Frequency
        if ((frames == firstframe) || (prevchn[c].note == -1) || (chn[c].freq != prevchn[c].freq))
        {
          int d;
          int dist = 0x7fffffff;
          int delta = ((int)chn[c].freq) - ((int)prevchn2[c].freq);

          sprintf(&output[strlen(output)], "%04X ", chn[c].freq);

          if (chn[c].wave >= 0x10)
          {
            // Get new note number
            for (d = 0; d < 96; d++)
            {
              int cmpfreq = freqtbllo[d] | (freqtblhi[d] << 8);
              int freq = chn[c].freq;

              if (abs(freq - cmpfreq) < dist)
              {
                dist = abs(freq - cmpfreq);
                // Favor the old note
                if (d == prevchn[c].note) dist /= oldnotefactor;
                 chn[c].note = d;
              }
            }

            // Print new note
            if (chn[c].note != prevchn[c].note)
            {
              if (prevchn[c].note == -1)
               {
                 if (lowres) newnote = 1;
                 sprintf(&output[strlen(output)], " %s %02X  ", notename[chn[c].note], chn[c].note | 0x80);
              }
               else
                sprintf(&output[strlen(output)], "(%s %02X) ", notename[chn[c].note], chn[c].note | 0x80);
            }
            else
            {
              // If same note, print frequency change (slide/vibrato)
              if (delta)
              {
                if (delta > 0)
                   sprintf(&output[strlen(output)], "(+ %04X) ", delta);
                 else
                   sprintf(&output[strlen(output)], "(- %04X) ", -delta);
              }
              else sprintf(&output[strlen(output)], " ... ..  ");
            }
          }
          else sprintf(&output[strlen(output)], " ... ..  ");
        }
        else sprintf(&output[strlen(output)], "....  ... ..  ");

        // Waveform
        if ((frames == firstframe) || (newnote) || (chn[c].wave != prevchn[c].wave))
          sprintf(&output[strlen(output)], "%02X ", chn[c].wave);
        else sprintf(&output[strlen(output)], ".. ");

        // ADSR
        if ((frames == firstframe) || (newnote) || (chn[c].adsr != prevchn[c].adsr)) sprintf(&output[strlen(output)], "%04X ", chn[c].adsr);
        else sprintf(&output[strlen(output)], ".... ");

        // Pulse
        if ((frames == firstframe) || (newnote) || (chn[c].pulse != prevchn[c].pulse)) sprintf(&output[strlen(output)], "%03X ", chn[c].pulse);
        else sprintf(&output[strlen(output)], "... ");

        sprintf(&output[strlen(output)], "| ");
      }

      // Filter cutoff
      if ((frames == firstframe) || (filt.cutoff != prevfilt.cutoff)) sprintf(&output[strlen(output)], "%04X ", filt.cutoff);
      else sprintf(&output[strlen(output)], ".... ");

      // Filter control
      if ((frames == firstframe) || (filt.ctrl != prevfilt.ctrl))
        sprintf(&output[strlen(output)], "%02X ", filt.ctrl);
      else sprintf(&output[strlen(output)], ".. ");

      // Filter passband
      if ((frames == firstframe) || ((filt.type & 0x70) != (prevfilt.type & 0x70)))
        sprintf(&output[strlen(output)], "%s ", filtername[(filt.type >> 4) & 0x7]);
      else sprintf(&output[strlen(output)], "... ");

      // Mastervolume
      if ((frames == firstframe) || ((filt.type & 0xf) != (prevfilt.type & 0xf))) sprintf(&output[strlen(output)], "%01X ", filt.type & 0xf);
      else sprintf(&output[strlen(output)], ". ");
      
      // Rasterlines / cycle count
      if (profiling)
      {
        int cycles = cpucycles;
        int rasterlines = (cycles + 62) / 63;
        int badlines = ((cycles + 503) / 504);
        int rasterlinesbad = (badlines * 40 + cycles + 62) / 63;
        sprintf(&output[strlen(output)], "| %4d %02X %02X ", cycles, rasterlines, rasterlinesbad);
      }
      
      // End of frame display, print info so far and copy SID registers to old registers
      sprintf(&output[strlen(output)], "|\n");
      if ((!lowres) || (!((frames - firstframe) % spacing[tempoindex])))
      {
        printf("%s", output);
        for (c = 0; c < 3; c++)
        {
          prevchn[c] = chn[c];
        }
        prevfilt = filt;
      }
      for (c = 0; c < 3; c++) prevchn2[c] = chn[c];

      // Print note/pattern separators
      if (spacing[tempoindex])
      {
        counter++;
        if (counter >= spacing[tempoindex])
        {
          tempoindex ^= 1;
          counter = 0;
          if (pattspacing)
          {
            rows++;
            if (rows >= pattspacing)
            {
              rows = 0;
              printf("+=======+===========================+===========================+===========================+===============+\n");
            }
            else
              if (!lowres) printf("+-------+---------------------------+---------------------------+---------------------------+---------------+\n");
          }
          else
            if (!lowres) printf("+-------+---------------------------+---------------------------+---------------------------+---------------+\n");
        }
      }
    }

    // Advance to next frame
    frames++;
  }
  return 0;
}

unsigned char readbyte(FILE *f)
{
  unsigned char res;

  fread(&res, 1, 1, f);
  return res;
}

unsigned short readword(FILE *f)
{
  unsigned char res[2];

  fread(&res, 2, 1, f);
  return (res[0] << 8) | res[1];
}





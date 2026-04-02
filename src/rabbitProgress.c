/* Copyright (C) NHR@FAU, University Erlangen-Nuremberg.
 * All rights reserved. This file is part of RabbitCT.
 * Use of this source code is governed by a MIT style
 * license that can be found in the LICENSE file. */
#include <stdio.h>
#include <stdlib.h>

#include "rabbitProgress.h"

static const char **mRabbit;
static int mMax                   = 0;
static int mNextLine              = 1;
static int mRabbitLines           = 0;

static const char *rabbit128[3]   = { " (\\_/)", "(='.'=)", "(\")_(\")" };

static const char *rabbit256[9]   = { "   _     _     ",
  "   \\`\\ /`/   ",
  "    \\ V /     ",
  "    /. .\\     ",
  "   =\\ T /=    ",
  "    / ^ \\     ",
  "   /\\\\ //\\  ",
  " __\\ \" \" /__",
  "(____/^\\____) " };

static const char *rabbit512[16]  = { "    / \\     / \\     ",
  "   {   }   {   }      ",
  "   {   {   }   }      ",
  "    \\   \\ /   /     ",
  "     \\   Y   /       ",
  "     .-\"`\"`\"-.     ",
  "   ,`         `.      ",
  "  /             \\    ",
  " /               \\   ",
  "{     ;\"\";,       } ",
  "{  /\";`'`,;       }  ",
  " \\{  ;`,'`;.     /   ",
  "  {  }`"
  "`  }   /}    ",
  "  {  }      {  //     ",
  "  {||}      {  /      ",
  "  `\"'       `\"'     " };

static const char *rabbit1024[19] = { "        /|      __    ",
  "       / |   ,-~ /    ",
  "      Y :|  //  /     ",
  "      | jj /( .^      ",
  "      >-\"~\"-v\"     ",
  "     /       Y        ",
  "    jo  o    |        ",
  "   ( ~T~     j        ",
  "    >._-' _./         ",
  "   /   \"~\"  |       ",
  "  Y     _,  |         ",
  " /| ;-\"~ _  l        ",
  "/ l/ ,-\"~    \\      ",
  "\\//\\/      .- \\    ",
  " Y        /    Y*     ",
  " l       I     !      ",
  " ]\\      _\\    /\"\\",
  "(\" ~----( ~   Y.  )  ",
  "~~~~~~~~~~~~~~~~~~~   " };

void rabbitProgress_init(int maxValue, int rabbitSize)
{
  mMax = maxValue;

  if (rabbitSize >= 1024) {
    mRabbit      = rabbit1024;
    mRabbitLines = 19;
  } else if (rabbitSize >= 512) {
    mRabbit      = rabbit512;
    mRabbitLines = 16;
  } else if (rabbitSize >= 256) {
    mRabbit      = rabbit256;
    mRabbitLines = 9;
  } else {
    mRabbit      = rabbit128;
    mRabbitLines = 3;
  }
}

void rabbitProgress_progress(int pos)
{
  if ((float)mNextLine / (float)mRabbitLines <= (float)pos / (float)mMax) {
    printf(" %s\n", mRabbit[mNextLine - 1]);
    mNextLine++;
  }
}

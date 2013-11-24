// Emacs style mode select -*- C++ -*-
//---------------------------------------------------------------------------
//
// Copyright(C) 2006 Simon Howard, James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//--------------------------------------------------------------------------
//
// Menu file selector
//
// eg. For selecting a wad to load or demo to play
//
// By Simon Howard
// Revisions by James Haley (taken from SMMU v3.30 8/11/00 build)
//
//---------------------------------------------------------------------------

#ifdef _MSC_VER
// for Visual C++:
#include "Win32/i_opndir.h"
#else
// for SANE compilers:
#include <dirent.h>
#endif

#include "z_zone.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "d_event.h"
#include "d_main.h"
#include "d_gi.h"
#include "d_io.h"
#include "doomstat.h"
#include "g_bind.h"
#include "m_misc.h"
#include "mn_engin.h"
#include "mn_files.h"
#include "mn_misc.h"
#include "r_data.h"
#include "s_sound.h"
#include "v_font.h"
#include "v_block.h"
#include "v_misc.h"
#include "v_video.h"
#include "w_wad.h"

//=============================================================================
//
// Read Directory
//
// Read all the files in a particular directory that fit a particular
// wildcard
//

//
// filecmp
//
// Compares a filename with a wildcard string.
//
static bool filecmp(const char *filename, const char *wildcard)
{
   static char no_ext[] = "";

   char *filename_main, *wildcard_main; // filename
   char *filename_ext, *wildcard_ext;   // extension
   bool res = true;
   int i = 0;
  
   // haleyjd: must be case insensitive
   filename_main = M_Strupr(estrdup(filename));
   wildcard_main = M_Strupr(estrdup(wildcard));
   
   // find separator -- haleyjd: use strrchr, not strchr
   filename_ext = strrchr(filename_main, '.');
   
   if(!filename_ext) 
      filename_ext = no_ext; // no extension
   else
   {
      // break into 2 strings; replace . with \0
      *filename_ext++ = '\0';
   }
   
   // do the same with the wildcard -- haleyjd: strrchr again  
   wildcard_ext = strrchr(wildcard_main, '.');

   if(!wildcard_ext)
      wildcard_ext = no_ext;
   else
      *wildcard_ext++ = '\0';

   // compare main part of filename with wildcard
   while(wildcard_main[i])
   {
      bool exitloop = false;

      switch(wildcard_main[i])
      {
      case '?':
         continue;
      case '*':
         exitloop = true;
         break;
      default:
         if(wildcard_main[i] != filename_main[i])
         {
            res = false;
            exitloop = true;
         }
         break;
      }

      if(exitloop)
         break;

      ++i;
   }

   // if first part of comparison fails, don't do 2nd part
   if(res)
   {
      i = 0;

      // compare extension
      while(wildcard_ext[i])
      {
         bool exitloop = false;
         
         switch(wildcard_ext[i])
         {
         case '?':
            continue;
         case '*':
            exitloop = true;
            break;
         default:
            if(wildcard_ext[i] != filename_ext[i])
            {
               res = false;
               exitloop = true;
            }
            break;
         }
         
         if(exitloop)
            break;
         
         ++i;
      }
   }

   // done with temporary strings
   efree(filename_main);
   efree(wildcard_main);

   return res;
}

//
// MN_addFile
//
// Adds a filename to the reallocating filenames array
//
static void MN_addFile(mndir_t *dir, const char *filename)
{
   if(dir->numfiles >= dir->numfilesalloc)
   {
      // realloc bigger: limitless
      dir->numfilesalloc = dir->numfilesalloc ? dir->numfilesalloc * 2 : 32;
      dir->filenames = erealloc(char **, dir->filenames, 
                                dir->numfilesalloc * sizeof(char *));
   }

   (dir->filenames)[dir->numfiles++] = estrdup(filename);
}

//
// MN_findFile
//
// Looks for the given file name in the list of filenames.
// Returns num_mn_files if not found.
//
static int MN_findFile(mndir_t *dir, const char *filename)
{
   int i;

   for(i = 0; i < dir->numfiles; i++)
   {
      if(!strcasecmp(filename, (dir->filenames)[i]))
         break;
   }

   return i;
}

//
// MN_ClearDirectory
//
// Empties out the reallocating filenames array
//
// haleyjd 06/15/10: made global
//
void MN_ClearDirectory(mndir_t *dir)
{
   // clear all alloced files   
   for(int i = 0; i < dir->numfiles; i++)
   {
      efree((dir->filenames)[i]);
      (dir->filenames)[i] = NULL;
   }

   dir->numfiles = 0;
}

//
// MN_qFileCompare
//
// qsort callback to compare file names
//
static int MN_qFileCompare(const void *si1, const void *si2)
{
   const char *str1 = *((const char **)si1);
   const char *str2 = *((const char **)si2);

   return strcasecmp(str1, str2);
}

//
// MN_sortFiles
//
// Sorts the directory listing.
//
static void MN_sortFiles(mndir_t *dir)
{
   if(dir->numfiles >= 2)
      qsort(dir->filenames, dir->numfiles, sizeof(char *), MN_qFileCompare);
}

//
// MN_ReadDirectory
//
// Uses POSIX opendir to read the indicated directory and saves off all file
// names within it that match the provided wildcard string.
//
// Note: for Visual C++, customized versions of MinGW's opendir functions
// are used, which are implemented in i_opndir.c.
//
// haleyjd 06/15/10: made global
//
int MN_ReadDirectory(mndir_t *dir, const char *read_dir, const char *read_wildcard)
{
   DIR *directory;
   struct dirent *direntry;

   // clear directory
   MN_ClearDirectory(dir);
  
   // open directory and read filenames  
   dir->dirpath = read_dir;
   directory = opendir(dir->dirpath);

   // test for failure
   if(!directory)
      return errno;
  
   while((direntry = readdir(directory)))
   {
      if(filecmp(direntry->d_name, read_wildcard))
         MN_addFile(dir, direntry->d_name); // add file to list
   }

   // done with directory listing
   closedir(directory);

   // sort the list
   MN_sortFiles(dir);

   return 0;
}

//=============================================================================
//
// File Selector
//
// Used as a 'browse' function when we are selecting some kind of file
// to load: eg. lmps, wads
//

static mndir_t *mn_currentdir;

static void MN_FileDrawer();
static bool MN_FileResponder(event_t *ev, int action);

// file selector is handled using a menu widget

static menuwidget_t file_selector = { MN_FileDrawer, MN_FileResponder, NULL, true };
static int selected_item;
static const char *variable_name;
static const char *help_description;
static int numfileboxlines;
static bool select_dismiss;
static bool allow_exit = true;

//
// MN_FileDrawer
//
// Drawer for the file selection dialog box. Easily the most complex
// graphical widget in the game engine so far. Significantly different
// than it was in SMMU 3.30.
//
static void MN_FileDrawer()
{
   int i;
   int bbot, btop, bleft, bright, w, h; // color box coords and dimensions
   int x, y;                            // text drawing coordinates
   int min, max, lheight;               // first & last line, and height of a
                                        // single line

   // draw a background
   V_DrawBackground(mn_background_flat, &vbscreen);

   // draw box
   V_DrawBox(16, 16, SCREENWIDTH - 32, SCREENHEIGHT - 32);

   // calculate height of one text line
   lheight = menu_font->absh;

   // calculate unscaled top, bottom, and height of color box
   btop = 16 + 8 + lheight + 8;
   bbot = SCREENHEIGHT - 16 - 8;
   h = bbot - btop + 1;
   
   // calculate unscaled left, right, and width of color box
   bleft  = 16 + 8;
   bright = SCREENWIDTH - 16 - 8;
   w = bright - bleft + 1;

   // draw color rect -- must manually scale coordinates
   // SoM: w = x2 - x1 + 1
   V_ColorBlock(&subscreen43, GameModeInfo->blackIndex,
                subscreen43.x1lookup[bleft], subscreen43.y1lookup[btop], 
                subscreen43.x2lookup[bleft + w - 1] - subscreen43.x1lookup[bleft] + 1,
                subscreen43.y2lookup[btop + h - 1] - subscreen43.y1lookup[btop] + 1);

   // draw the dialog title
   if(help_description)
      MN_WriteTextColored(help_description, CR_GOLD, 16 + 8, 16 + 8);

   // add one to the line height for the remaining lines to leave a gap
   lheight += 1;
   
   // calculate the number of text lines that can fit in the box
   // leave 2 pixels of gap for the top and the bottom of the box
   numfileboxlines = (h - 2) / lheight;

   // determine first and last filename to display (we can display
   // "numlines" filenames.
   // haleyjd 11/04/06: improved to keep selection centered in box except at
   // beginning and end of list.
   min = selected_item - numfileboxlines/2 /* + 1*/;
   if(min < 0)
      min = 0;
   max = min + numfileboxlines - 1;
   if(max >= mn_currentdir->numfiles)
   {
      max = mn_currentdir->numfiles - 1;
      // haleyjd 11/04/06: reset min when the list end is displayed to
      // keep the box completely full.
      min = max - numfileboxlines + 1;
      if(min < 0)
         min = 0;
   }

   // start 1 pixel right & below the color box coords
   x = bleft + 1;
   y = btop + 1;

   // draw the file names starting from min and going to max
   for(i = min; i <= max; ++i)
   {
      int color;
      const char *text;

      // if this is the selected item, use the appropriate color
      if(i == selected_item)
      {
         color = GameModeInfo->selectColor;
         MN_DrawSmallPtr(x + 1, y + lheight / 2 - 4);
      }
      else
         color = GameModeInfo->unselectColor;

      // haleyjd 11/04/06: add "more" indicators when box is scrolled :)
      if((i == min && min > 0) || (i == max && max < mn_currentdir->numfiles - 1))
         text = FC_GOLD "More...";
      else
         text = (mn_currentdir->filenames)[i];
      
      // draw it!
      MN_WriteTextColored(text, color, x + 11, y);

      // step by the height of one line
      y += lheight;
   }
}

//
// MN_doExitFileWidget
//
// When allow_exit flag is false, call D_StartTitle
//
static void MN_doExitFileWidget()
{
   MN_PopWidget();  // cancel widget
   MN_ClearMenus();
   D_StartTitle();
}

//
// MN_FileResponder
//
// Responds to events for the file selection dialog widget. Uses
// keybinding actions rather than key constants like in SMMU. Also
// added sounds to give a more consistent UI feel.
//
static bool MN_FileResponder(event_t *ev, int action)
{
   unsigned char ch;

   if(action == ka_menu_up || action == ka_menu_left)
   {
      if(selected_item > 0) 
      {
         selected_item--;
         S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_KEYUPDOWN]);
      }
      return true;
   }
  
   if(action == ka_menu_down || action == ka_menu_right)
   {
      if(selected_item < (mn_currentdir->numfiles - 1)) 
      {
         selected_item++;
         S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_KEYUPDOWN]);
      }
      return true;
   }
   
   if(action == ka_menu_pageup)
   {
      if(numfileboxlines)
      {
         selected_item -= numfileboxlines;
         if(selected_item < 0)
            selected_item = 0;
         S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_KEYLEFTRIGHT]);
      }
      return true;
   }
  
   if(action == ka_menu_pagedown)
   {
      if(numfileboxlines)
      {
         selected_item += numfileboxlines;
         if(selected_item >= mn_currentdir->numfiles) 
            selected_item = mn_currentdir->numfiles - 1;
         S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_KEYLEFTRIGHT]);
      }
      return true;
   }
   
   if(action == ka_menu_toggle || action == ka_menu_previous)
   {
      // When allow_exit flag is false, call D_StartTitle
      if(!allow_exit)
      {
         MN_QuestionFunc("Are you sure you want to exit?\n\n(Press y to exit)", 
                         MN_doExitFileWidget);
         S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_ACTIVATE]);
      }
      else
      {
         MN_PopWidget(); // cancel widget
         S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_DEACTIVATE]);
      }
      return true;
   }
  
   if(action == ka_menu_confirm)
   {
      // set variable to new value
      if(variable_name)
      {
         char tempstr[128];
         psnprintf(tempstr, sizeof(tempstr), 
            "%s \"%s\"", variable_name, (mn_currentdir->filenames)[selected_item]);
         C_RunTextCmd(tempstr);
         S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_COMMAND]);
      }
      if(select_dismiss)
         MN_PopWidget(); // cancel widget
      return true;
   }

   // search for matching item in file list

   if(ev->character)
      ch = tolower((unsigned char)(ev->character));
   else
      ch = tolower(ev->data1);

   if(ch >= 'a' && ch <= 'z')
   {  
      int n = selected_item;
      
      do
      {
         n++;
         if(n >= mn_currentdir->numfiles) 
            n = 0; // loop round
         
         auto uch = (unsigned char)((mn_currentdir->filenames)[n][0]);
         if(tolower(uch) == ch)
         {
            // found a matching item!
            if(n != selected_item) // only make sound if actually moving
            {
               selected_item = n;
               S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_KEYUPDOWN]);
            }
            return true; // eat key
         }
      } 
      while(n != selected_item);
   }
   
   return false; // not interested
}

char *wad_directory; // directory where user keeps wads

VARIABLE_STRING(wad_directory,  NULL,          1024);
CONSOLE_VARIABLE(wad_directory, wad_directory, cf_allowblank)
{
   // normalize slashes
   M_NormalizeSlashes(wad_directory);
}

static mndir_t mn_diskdir;

CONSOLE_COMMAND(mn_selectwad, 0)
{
   int ret = MN_ReadDirectory(&mn_diskdir, wad_directory, "*.wad");

   // check for standard errors
   if(ret)
   {
      const char *msg = strerror(ret);

      MN_ErrorMsg("opendir error: %d (%s)", ret, msg);
      return;
   }

   // empty directory?
   if(mn_diskdir.numfiles < 1)
   {
      MN_ErrorMsg("no files found");
      return;
   }

   selected_item      = 0;
   mn_currentdir      = &mn_diskdir;
   help_description   = "select wad file:";
   variable_name      = "mn_wadname";
   select_dismiss     = true;
   allow_exit         = true;

   MN_PushWidget(&file_selector);
}

//
// MN_DisplayFileSelector
//
// haleyjd 06/16/10: for external access to the file selector widget
//
void MN_DisplayFileSelector(mndir_t *dir, const char *title, 
                            const char *command, bool dismissOnSelect,
                            bool allowExit)
{
   if(dir->numfiles < 1)
      return;

   selected_item    = 0;
   mn_currentdir    = dir;
   help_description = title;
   variable_name    = command;
   select_dismiss   = dismissOnSelect;
   allow_exit       = allowExit;

   MN_PushWidget(&file_selector);
}

//=============================================================================
//
// Misc stuff
//
// The Filename reading provides a useful opportunity for some handy
// console commands.
//
  
// 'dir' command is useful :)

CONSOLE_COMMAND(dir, 0)
{
   int i;
   const char *wildcard;
   
   if(Console.argc)
      wildcard = Console.argv[0]->constPtr();
   else
      wildcard = "*.*";
   
   MN_ReadDirectory(&mn_diskdir, ".", wildcard);
   
   for(i = 0; i < mn_diskdir.numfiles; ++i)
   {
      C_Printf("%s\n", (mn_diskdir.filenames)[i]);
   }
}

CONSOLE_COMMAND(mn_selectmusic, 0)
{
   musicinfo_t *music;
   char namebuf[16];
   int i;

   // clear directory
   MN_ClearDirectory(&mn_diskdir);

   // run down music hash chains and add music to file list
   for(i = 0; i < SOUND_HASHSLOTS; ++i)
   {
      music = musicinfos[i];

      while(music)
      {
         // don't add music entries that don't actually exist
         if(music->prefix)
         {
            psnprintf(namebuf, sizeof(namebuf), "%s%s", 
                      GameModeInfo->musPrefix, music->name);
         }
         else
            psnprintf(namebuf, sizeof(namebuf), "%s", music->name);
         
         if(W_CheckNumForName(namebuf) >= 0)
            MN_addFile(&mn_diskdir, music->name);

         music = music->next;
      }
   }

   if(mn_diskdir.numfiles < 1)
   {
      C_Printf(FC_ERROR "No music found");
      return;
   }

   // sort the list
   MN_sortFiles(&mn_diskdir);

   selected_item    = 0;
   mn_currentdir    = &mn_diskdir;
   help_description = "select music to play:";
   variable_name    = "s_playmusic";
   select_dismiss   = false;
   allow_exit       = true;

   MN_PushWidget(&file_selector);
}

CONSOLE_COMMAND(mn_selectflat, 0)
{
   int i;
   int curnum;

   // clear directory
   MN_ClearDirectory(&mn_diskdir);

   MN_addFile(&mn_diskdir, "default");

   // run through flats
   for(i = flatstart; i < flatstop; i++)
   {
      // size must be exactly 64x64
      if(textures[i]->width == 64 && textures[i]->height == 64)
         MN_addFile(&mn_diskdir, textures[i]->name);
   }

   if(mn_diskdir.numfiles < 1)
   {
      MN_ErrorMsg("No flats found");
      return;
   }

   // sort the list
   MN_sortFiles(&mn_diskdir);
   
   selected_item = 0;

   if((curnum = MN_findFile(&mn_diskdir, mn_background)) != mn_diskdir.numfiles)
      selected_item = curnum;
   
   mn_currentdir    = &mn_diskdir;
   help_description = "select background:";
   variable_name    = "mn_background";
   select_dismiss   = false;
   allow_exit       = true;

   MN_PushWidget(&file_selector);
}

// EOF


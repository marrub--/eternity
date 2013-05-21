// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 James Haley
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
// DESCRIPTION:
//  Patch format verification and load-time processing code
//
//-----------------------------------------------------------------------------

#ifndef V_PATCHFMT_H__
#define V_PATCHFMT_H__

#include "w_wad.h"

struct patch_t;

class PatchLoader : public WadLumpLoader
{
private:
   static size_t   DefaultPatchSize;
   static patch_t *GetDefaultPatch();

   bool checkData(void *data, size_t size) const;
   void formatRaw(void *data) const;

public:
   // A global instance of PatchLoader for passing to WadDirectory methods
   static PatchLoader patchFmt;

   virtual Code verifyData(lumpinfo_t *lump) const;
   virtual Code formatData(lumpinfo_t *lump) const;
   virtual lumpinfo_t::lumpformat formatIndex() const { return lumpinfo_t::fmt_patch; }

   static patch_t *CacheName(WadDirectory &dir, const char *name, int tag);
   static patch_t *CacheNum(WadDirectory &dir, int lumpnum, int tag);

   static bool VerifyAndFormat(void *data, size_t size);
   static void GetUsedColors(patch_t *patch, byte *pal);
};

#endif

// EOF

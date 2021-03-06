/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef __khRasterProductLevel_h
#define __khRasterProductLevel_h

#include <khGuard.h>
#include "pyrio.h"
#include <notify.h>
#include <khTileAddr.h>
#include <khException.h>


/******************************************************************************
 ***  khRasterProductLevel - a single level of a khRasterProduct
 ***
 ***  The disk memory of these objects are the old .pyr files.
 ***
 ***  Usage:
 ***    khRasterProductLevel &level(rp->level(5));
 ***    notify(NFY_DEBUG, "Level 5 has %d x %d tiles",
 ***           level.xNumTiles(), level.yNumTiles());
 ***    level.OpenReader();
 ***    for (...) {
 ***      level.ReadTile(row, col, destTile);
 ***    }
 ***    level.CloseReader();
 ***
 ***    - or -
 ***
 ***    (*rp)[5].OpenReader();
 ***    for (...) {
 ***      (*rp)[5].ReadTile(row, col, destTile);
 ***    }
 ***    (*rp)[5].CloseReader();
 ***
 ******************************************************************************/

class khRasterProductLevel
{
  friend class khRasterProduct;

  khRasterProduct *product_;
  khLevelCoverage coverage;
  mutable khDeleteGuard<pyrio::Reader> reader;
  mutable khDeleteGuard<pyrio::Writer> writer;
  CompressDef compdef;

  khRasterProductLevel(khRasterProduct *product,
                       const khLevelCoverage &coverage_,
                       const CompressDef &compdef_) :
      product_(product),
      coverage(coverage_),
      reader(),
      writer(),
      compdef(compdef_)
  {
  }

  // private and unimplemented - Don't copy these
  khRasterProductLevel(const khRasterProductLevel &);
  khRasterProductLevel& operator=(const khRasterProductLevel&);
 public:
  inline const khRasterProduct* product(void) const { return product_;}
  inline uint levelnum(void) const { return coverage.level; }
  std::string filename(void) const;
  uint numComponents(void) const;
  khTypes::StorageEnum componentType(void) const;


  bool OpenReader(void) const;
  void CloseReader(void) const { reader.clear(); /* will delete existing */ }
  bool OpenWriter(const bool is_monochromatic) const;
  void CloseWriter(void) const { writer.clear(); /* will delete existing */ }

  // Requests outside range will return false.
  // Data will be coerced to target type
  template <class DestTile>
  inline bool ReadTile(uint32 row, uint32 col, DestTile &dest) const;

  inline bool IsMonoChromatic() const {
    if (!reader && !OpenReader()) {
      return false;
    }
    return (reader->header().numComponents == 1);
  }

  bool
  ReadTileBandIntoBuf(uint32 row, uint32 col, uint band,
                      uchar* destBuf,
                      khTypes::StorageEnum destType,
                      uint bufsize,
                      bool flipTopToBottom = false) const;

  // will throw if unable to read entire extents
  void
  ReadImageIntoBufs(const khExtents<uint32> &pixelExtents,
                    uchar *destBufs[],
                    uint   bands[],
                    uint   numbands,
                    khTypes::StorageEnum destType) const;


  // Requests outside range will return false.
  // Data must match the type specified in the parent product
  template <class SrcTile>
  bool WriteTile(uint32 row, uint32 col, const SrcTile &src);

  // extents of level in level-wide row/col
  inline const khExtents<uint32>& tileExtents(void) const {
    return coverage.extents;
  }
  inline const khExtents<uint64> tilePixelExtents(void) const {
    return khExtents<uint64>(RowColOrder,
                             (uint64)tileExtents().beginRow() *
                             (uint64)RasterProductTileResolution,
                             (uint64)tileExtents().endRow() *
                             (uint64)RasterProductTileResolution,
                             (uint64)tileExtents().beginCol() *
                             (uint64)RasterProductTileResolution,
                             (uint64)tileExtents().endCol() *
                             (uint64)RasterProductTileResolution);
  }


};

/******************************************************************************
 ***  Inlines from khRasterProduct::Level
 ******************************************************************************/

template <class DestTile>
inline bool
khRasterProductLevel::ReadTile(uint32 row, uint32 col, DestTile &dest) const
{
  COMPILE_TIME_ASSERT(DestTile::TileWidth == RasterProductTileResolution,
                      IncompatibleTileWidth);
  COMPILE_TIME_ASSERT(DestTile::TileHeight == RasterProductTileResolution,
                      IncompatibleTileHeight);
  if (DestTile::NumComp != numComponents()) {
    notify(NFY_WARN, "Internal Error: Invalid number of bands: %u != %u",
           DestTile::NumComp, numComponents());
    return false;
  }
  if (!tileExtents().ContainsRowCol(row, col)) {
    notify(NFY_WARN, "Internal Error: Invalid row/col: %u,%u", row, col);
    return false;
  }


  if (!reader && !OpenReader()) return false;
  return reader->ReadAllBandBufs(row - tileExtents().beginRow(),
                                 col - tileExtents().beginCol(),
                                 dest.ucharBufs,
                                 DestTile::Storage, DestTile::NumComp);
}


#endif /* __khRasterProductLevel_h */

/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2017-2020, Regents of the University of California.
 *
 * This file is part of ndncert, a certificate management system based on NDN.
 *
 * ndncert is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * ndncert is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received copies of the GNU General Public License along with
 * ndncert, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ndncert authors and contributors.
 */

#include "error.hpp"

namespace ndn {
namespace ndncert {

Block
ERROR::encodeDataContent(Error errorCode, const std::string& description)
{
  Block response = makeEmptyBlock(tlv::Content);
  response.push_back(makeNonNegativeIntegerBlock(tlv_error_code, static_cast<size_t>(errorCode)));
  response.push_back(makeStringBlock(tlv_error_info, description));
  response.encode();
  return response;
}

std::tuple<Error, std::string>
ERROR::decodefromDataContent(const Block& block)
{
  block.parse();
  Error error = static_cast<Error>(readNonNegativeInteger(block.get(tlv_error_code)));
  auto description = readString(block.get(tlv_error_info));
  return std::make_tuple(error, description);
}

}  // namespace ndncert
}  // namespace ndn
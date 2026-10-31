/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "sherlock/resources.h"

namespace Sherlock {

void Resources::decompressLZ(Common::SeekableReadStream &source, byte *outBuffer, int32 outSize, int32 inSize) {
	byte lzWindow[4096];
	uint16 lzWindowPos;
	uint16 cmd;

	byte *outBufferEnd = outBuffer + outSize;
	int endPos = source.pos() + inSize;

	memset(lzWindow, 0xFF, 0xFEE);
	lzWindowPos = 0xFEE;
	cmd = 0;

	do {
		cmd >>= 1;
		if (!(cmd & 0x100))
			cmd = source.readByte() | 0xFF00;

		if (cmd & 1) {
			byte literal = source.readByte();
			*outBuffer++ = literal;
			lzWindow[lzWindowPos] = literal;
			lzWindowPos = (lzWindowPos + 1) & 0x0FFF;
		} else {
			int copyPos, copyLen;
			copyPos = source.readByte();
			copyLen = source.readByte();
			copyPos = copyPos | ((copyLen & 0xF0) << 4);
			copyLen = (copyLen & 0x0F) + 3;
			while (copyLen-- && (outSize == -1 || outBuffer < outBufferEnd)) {
				byte literal = lzWindow[copyPos];
				copyPos = (copyPos + 1) & 0x0FFF;
				*outBuffer++ = literal;
				lzWindow[lzWindowPos] = literal;
				lzWindowPos = (lzWindowPos + 1) & 0x0FFF;
			}
		}
	} while ((outSize == -1 || outBuffer < outBufferEnd) && (inSize == -1 || source.pos() < endPos));
	if (inSize != -1 && source.pos() < endPos) {
		source.skip(endPos - source.pos());
	}
	if (outSize != -1 && outBuffer < outBufferEnd) {
		memset(outBuffer, 0, outBufferEnd - outBuffer);
	}
}

} // End of namespace Sherlock

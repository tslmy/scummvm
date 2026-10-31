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

#include "sherlock/image_file.h"
#include "sherlock/resources.h"
#include "common/memstream.h"
#include "common/util.h"

namespace Sherlock {

void ImageFrame::decompressFrame(const byte *src, bool isRoseTattoo) {
	_frame.create(_width, _height, Graphics::PixelFormat::createFormatCLUT8());
	byte *dest = (byte *)_frame.getPixels();
	Common::fill(dest, dest + _width * _height, 0xff);

	if (_paletteBase) {
		// Nibble-packed
		for (uint idx = 0; idx < _size; ++idx, ++src) {
			*dest++ = *src & 0xF;
			*dest++ = (*src >> 4);
		}
	} else if (_rleEncoded && isRoseTattoo) {
		// Rose Tattoo run length encoding doesn't use the RLE marker byte
		for (int yp = 0; yp < _height; ++yp) {
			int xSize = _width;
			while (xSize > 0) {
				// Skip a given number of pixels
				byte skip = *src++;
				dest += skip;
				xSize -= skip;
				if (!xSize)
					break;

				// Get a run length, and copy the following number of pixels
				int rleCount = *src++;
				xSize -= rleCount;
				while (rleCount-- > 0)
					*dest++ = *src++;
			}
			assert(xSize == 0);
		}
	} else if (_rleEncoded) {
		// RLE encoded
		int frameSize = _width * _height;
		while (frameSize > 0) {
			if (*src == _rleMarker) {
				byte rleColor = src[1];
				byte rleCount = MIN((int)src[2], frameSize);
				src += 3;
				frameSize -= rleCount;
				while (rleCount--)
					*dest++ = rleColor;
			} else {
				*dest++ = *src++;
				--frameSize;
			}
		}
	} else {
		// Uncompressed frame
		Common::copy(src, src + _width * _height, dest);
	}
}

// The outer room stream may be uncompressed while its image frames are RLE
// or nibble-packed. Verify those payloads before the decoder consumes them.
static bool validUncompressedFrame(const ImageFrame &frame, const byte *data) {
	// Graphics::Surface dimensions are signed 16-bit values.
	if (!frame._width || !frame._height || frame._width > 0x7fff || frame._height > 0x7fff)
		return false;
	const uint32 pixels = (uint32)frame._width * frame._height;
	if (frame._paletteBase)
		return frame._size <= pixels / 2;
	if (!frame._rleEncoded)
		return pixels <= frame._size;

	uint32 remaining = frame._size;
	for (uint y = 0; y < frame._height; ++y) {
		uint rowRemaining = frame._width;
		while (rowRemaining) {
			if (!remaining)
				return false;
			const uint skip = *data++;
			--remaining;
			if (skip > rowRemaining)
				return false;
			rowRemaining -= skip;
			if (!rowRemaining)
				break;
			if (!remaining)
				return false;
			const uint run = *data++;
			--remaining;
			if (run > rowRemaining || run > remaining)
				return false;
			data += run;
			remaining -= run;
			rowRemaining -= run;
		}
	}
	return true;
}

StreamingImageFile::StreamingImageFile() {
	_frameNumber = 0;
	_stream = nullptr;
	_flags = 0;
	_scaleVal = 0;
	_zPlacement = 0;
	_compressed = false;
	_active = false;
}

StreamingImageFile::~StreamingImageFile() {
	close();
}

void StreamingImageFile::load(Common::SeekableReadStream *stream, bool compressed) {
	_stream = stream;
	_compressed = compressed;
	_frameNumber = -1;
	_active = true;
}

void StreamingImageFile::close() {
	delete _stream;
	_stream = nullptr;
	_frameNumber = -1;
	_active = false;
	_imageFrame._frame.free();
}

bool StreamingImageFile::getNextFrame() {
	// Don't proceed if we're already at the end of the stream
	assert(_stream);
	if (_stream->pos() >= _stream->size()) {
		_active = false;
		return false;
	}

	// Increment frame number
	++_frameNumber;

	// If necessary, decompress the next frame
	Common::SeekableReadStream *frameStream = _stream;
	if (_compressed) {
		uint32 inSize = _stream->readUint32LE();
		Resources::decompressLZ(*_stream, _buffer, STREAMING_BUFFER_SIZE, inSize);
		frameStream = new Common::MemoryReadStream(_buffer, 11, DisposeAfterUse::NO);
	}

	// Load the data for the frame
	_imageFrame._width = frameStream->readUint16LE() + 1;
	_imageFrame._height = frameStream->readUint16LE() + 1;
	_imageFrame._paletteBase = frameStream->readByte();
	_imageFrame._rleEncoded = frameStream->readByte() == 1;
	_imageFrame._offset.x = frameStream->readByte();
	_imageFrame._offset.y = frameStream->readByte();
	_imageFrame._size = frameStream->readUint16LE() - 11;
	_imageFrame._rleMarker = frameStream->readByte();

	// Free the previous frame
	_imageFrame._frame.free();

	// Decode the frame
	if (_compressed) {
		delete frameStream;
		_imageFrame.decompressFrame(_buffer + 11, true);
	} else {
		// Uncompressed frame data follows the 11-byte header in the input
		// stream. Put it in the same bounded scratch area used by LZ frames,
		// rather than decoding stale data left there by an earlier frame.
		if (_imageFrame._size > STREAMING_BUFFER_SIZE - 11 ||
				_stream->read(_buffer + 11, _imageFrame._size) != _imageFrame._size ||
				!validUncompressedFrame(_imageFrame, _buffer + 11)) {
			_active = false;
			return false;
		}
		_imageFrame.decompressFrame(_buffer + 11, true);
	}

	return true;
}

} // End of namespace Sherlock

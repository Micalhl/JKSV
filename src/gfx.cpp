#include <string>
#include <cstring>
#include <fstream>
#include <stdio.h>
#include <switch.h>

#include <malloc.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "gfx.h"

static FT_Error ret = 0, libret = 1, faceret = 1;
static FT_Library lib;
static FT_Face face;
static uint32_t frameBufWidth = 0;

namespace gfx
{
	bool init(const uint32_t& _fontSize)
	{
		Result res = 0;

		gfxInitDefault();
		consoleInit(NULL);

		PlFontData font;

		res = plInitialize();
		if(R_FAILED(res))
		{
			printf("plInitialize failed.");
			return false;
		}

		res = plGetSharedFontByType(&font, PlSharedFontType_KO);
		if(R_FAILED(res))
		{
			printf("plGetSharedFontByTypeFailed!");
			return false;
		}

		ret = FT_Init_FreeType(&lib);
		libret = ret;
		if(ret)
		{
			printf("FT_Init_FreeType() failed: %d", ret);
			return false;
		}

		ret = FT_New_Memory_Face(lib, (FT_Byte *)font.address, font.size, 0, &face);
		faceret = ret;
		if(ret)
		{
			printf("FT_New_Memory_Face failed: %d", ret);
			return false;
		}

		return true;
	}

	bool fini()
	{
		if(faceret == 0)
			FT_Done_Face(face);
		if(libret == 0)
			FT_Done_FreeType(lib);

		plExit();
		gfxExit();

		return true;
	}

	void switchMode()
	{
		gfxSetMode(GfxMode_LinearDouble);
	}

	void handleBuffs()
	{
		gfxFlushBuffers();
		gfxSwapBuffers();
		gfxWaitForVsync();
	}

	//switch-portlibs examples.
	void drawGlyph(FT_Bitmap& bmp, uint32_t *frameBuf, unsigned x, unsigned y, const uint32_t& clr)
	{
		uint32_t frameX, frameY, tmpX, tmpY;
		uint8_t *imgPtr = bmp.buffer;

		if(bmp.pixel_mode != FT_PIXEL_MODE_GRAY)
			return;

		uint8_t r, g, b;
		r = clr >> 24 & 0xFF;
		g = clr >> 16 & 0xFF;
		b = clr >> 8 & 0xFF;

		for(tmpY = 0; tmpY < bmp.rows; tmpY++)
		{
			for(tmpX = 0; tmpX < bmp.width; tmpX++)
			{
				frameX = x + tmpX;
				frameY = y + tmpY;

				if(imgPtr[tmpX] > 0)
				{
					uint32_t fbPx = frameBuf[frameY * frameBufWidth + frameX];
					uint32_t txPx = (r << 24 | g << 16 | b << 8 | imgPtr[tmpX]);
					frameBuf[frameY * frameBufWidth + frameX] = blend(txPx, fbPx);
				}
			}

			imgPtr += bmp.pitch;
		}
	}

	void drawText(const std::string& str, unsigned x, unsigned y, const uint32_t& sz, const uint32_t& clr)
	{
		//This offset needs to be fixed better
		y = y + 27;
		uint32_t tmpX = x;
		FT_Error ret = 0;
		FT_UInt glyphIndex;
		FT_GlyphSlot slot = face->glyph;
		uint32_t tmpChr;
		ssize_t unitCount = 0;

		FT_Set_Char_Size(face, 0, 8 * sz, 300, 300);

		uint32_t *frameBuffer = (uint32_t *)gfxGetFramebuffer(&frameBufWidth, NULL);

		uint8_t tmpStr[1024];
		sprintf((char *)tmpStr, "%s", str.c_str());

		for(unsigned i = 0; i < str.length(); )
		{
			unitCount = decode_utf8(&tmpChr, &tmpStr[i]);
			if(unitCount <= 0)
				break;

			i += unitCount;
			if(tmpChr == '\n')
			{
				tmpX = x;
				y += face->size->metrics.height / sz;
				continue;
			}

			glyphIndex = FT_Get_Char_Index(face, tmpChr);
			ret = FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT);
			if(ret == 0)
			{
				ret = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
			}

			if(ret)
				return;

			drawGlyph(slot->bitmap, frameBuffer, tmpX + slot->bitmap_left, y - slot->bitmap_top, clr);
			tmpX += slot->advance.x >> 6;
			y += slot->advance.y >> 6;
		}
	}

	unsigned getTextWidth(const std::string& str, const uint32_t& sz)
	{
		unsigned width = 0;

		uint32_t unitCount = 0, tmpChr = 0;
		FT_UInt glyphIndex = 0;
		FT_GlyphSlot slot = face->glyph;
		FT_Error ret = 0;

		uint8_t tmpstr[1024];
		sprintf((char *)tmpstr, "%s", str.c_str());

		FT_Set_Char_Size(face, 0, 8 * sz, 300, 300);

		for(unsigned i = 0; i < str.length(); )
		{
			unitCount = decode_utf8(&tmpChr, &tmpstr[i]);

			if(unitCount <= 0)
				break;

			i += unitCount;
			glyphIndex = FT_Get_Char_Index(face, tmpChr);
			ret = FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT);
			if(ret == 0)
			{
				ret = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
			}

			if(ret)
				return 0;

			width += slot->advance.x >> 6;
		}

		return width;
	}

	unsigned getTextHeight(const uint32_t& sz)
	{
		FT_UInt glyphIndex = 0;
		FT_Error error = 0;
		uint32_t tChar = 'A';

		FT_Set_Char_Size(face, 0, 8 * sz, 300, 300);
		glyphIndex = FT_Get_Char_Index(face, tChar);
		error = FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT);
		if(error == 0)
			error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);

		if(error)
			return 0;

		return face->size->metrics.height / sz;
	}

	void clearBufferColor(const uint32_t& clr)
	{
		uint8_t r, g, b;
		r = clr >> 24 & 0xFF;
		g = clr >> 16 & 0xFF;
		b = clr >> 8 & 0xFF;

		size_t fbSize = gfxGetFramebufferSize() / 4;
		uint32_t *fb = (uint32_t *)gfxGetFramebuffer(NULL, NULL);

		for(unsigned i = 0; i < fbSize; i++)
		{
			fb[i] = RGBA8_MAXALPHA(r, g, b);
		}
	}

	uint32_t blend(const uint32_t& clr, const uint32_t& fb)
	{
		uint8_t r1, g1, b1, al;
		r1 = clr >> 24 & 0xFF;
		g1 = clr >> 16 & 0xFF;
		b1 = clr >> 8  & 0xFF;
		al = clr & 0xFF;

		//Assuming this is FB
		uint8_t r2, g2, b2;
		r2 = fb & 0xFF;
		g2 = fb >> 8 & 0xFF;
		b2 = fb >> 16 & 0xFF;

		uint8_t subAl = (uint8_t)0xFF - al;

		uint8_t finalRed = (r1 * al + r2 * subAl) / 0xFF;
		uint8_t finalGreen = (g1 * al + g2 * subAl) / 0xFF;
		uint8_t finalBlue = (b1 * al + b2 * subAl) / 0xFF;

		return (0xFF << 24 | finalBlue << 16 | finalGreen << 8 | finalRed);
	}

	void drawRectangle(uint32_t x, uint32_t y, const uint32_t& width, const uint32_t& height, const uint32_t& clr)
	{
		uint32_t w, h, tX, tY;
		uint32_t *frameBuffer = (uint32_t *)gfxGetFramebuffer(&w, &h);

		uint8_t r, g, b;
		r = clr >> 24 & 0xFF;
		g = clr >> 16 & 0xFF;
		b = clr >> 8 & 0xFF;

		for(tY = y; tY < y + height; tY++)
		{
			for(tX = x; tX < x + width; tX++)
			{
				frameBuffer[tY * w + tX] = RGBA8_MAXALPHA(r, g, b);
			}
		}
	}

	void tex::loadFromFile(const std::string& path)
	{
		std::fstream dataIn(path, std::ios::in | std::ios::binary);
		if(dataIn.is_open())
		{
			dataIn.read((char *)&sz, sizeof(uint32_t));
			dataIn.read((char *)&width, sizeof(uint16_t));
			dataIn.read((char *)&height, sizeof(uint16_t));

			data = new uint8_t[sz];
			if(data != NULL)
				dataIn.read((char *)data, sz);

			dataIn.close();
		}
	}

	tex::~tex()
	{
		if(data != NULL)
			delete[] data;
	}

	void tex::draw(uint32_t x, uint32_t y)
	{
		if(data != NULL)
		{
			uint32_t tY, tX, i = 0;
			uint32_t *frameBuffer = (uint32_t *)gfxGetFramebuffer(NULL, NULL);

			for(tY = y; tY < y + height; tY++)
			{
				for(tX = x; tX < x + width; tX++, i += 4)
				{
					uint32_t clr = (data[i] << 24 | data[i + 1] << 16 | data[i + 2] << 8 | data[i + 3]);
					uint32_t buf = frameBuffer[tY * frameBufWidth + tX];
					frameBuffer[tY * frameBufWidth + tX] = blend(clr, buf);
				}
			}
		}
	}
}

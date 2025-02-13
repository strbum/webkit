/*
 * Copyright (C) 2008, 2013 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Image.h"
#include "BitmapImage.h"
#include "GraphicsContextImplCairo.h"
#include "RefPtrCairo.h"
#include <cairo.h>
#include <cairo-win32.h>

#include <windows.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

RefPtr<BitmapImage> BitmapImage::create(HBITMAP hBitmap)
{
    DIBSECTION dibSection;
    if (!GetObject(hBitmap, sizeof(DIBSECTION), &dibSection))
        return 0;

    ASSERT(dibSection.dsBm.bmBitsPixel == 32);
    if (dibSection.dsBm.bmBitsPixel != 32)
        return 0;

    ASSERT(dibSection.dsBm.bmBits);
    if (!dibSection.dsBm.bmBits)
        return 0;

    RefPtr<cairo_surface_t> surface = adoptRef(cairo_win32_surface_create_with_dib(CAIRO_FORMAT_ARGB32, dibSection.dsBm.bmWidth, dibSection.dsBm.bmHeight));

    return BitmapImage::create(WTFMove(surface));
}

bool BitmapImage::getHBITMAPOfSize(HBITMAP bmp, const IntSize* size)
{
    ASSERT(bmp);

    BITMAP bmpInfo;
    GetObject(bmp, sizeof(BITMAP), &bmpInfo);

    // If this is a 32bpp bitmap, which it always should be, we'll clear it so alpha-wise it will be visible
    if (bmpInfo.bmBitsPixel == 32 && bmpInfo.bmBits) {
        int bufferSize = bmpInfo.bmWidthBytes * bmpInfo.bmHeight;
        memset(bmpInfo.bmBits, 255, bufferSize);
    }

    unsigned char* bmpdata = (unsigned char*)bmpInfo.bmBits + bmpInfo.bmWidthBytes*(bmpInfo.bmHeight-1);

    cairo_surface_t* image = cairo_image_surface_create_for_data(bmpdata, CAIRO_FORMAT_ARGB32, bmpInfo.bmWidth, bmpInfo.bmHeight, -bmpInfo.bmWidthBytes);

    cairo_t* targetRef = cairo_create(image);
    cairo_surface_destroy(image);

    GraphicsContext gc(GraphicsContextImplCairo::createFactory(targetRef));

    FloatSize imageSize = BitmapImage::size();
    if (size)
        drawFrameMatchingSourceSize(gc, FloatRect(0.0f, 0.0f, bmpInfo.bmWidth, bmpInfo.bmHeight), *size, CompositeCopy);
    else
        draw(gc, FloatRect(0.0f, 0.0f, bmpInfo.bmWidth, bmpInfo.bmHeight), FloatRect(0.0f, 0.0f, imageSize.width(), imageSize.height()), CompositeCopy, BlendMode::Normal, DecodingMode::Synchronous, ImageOrientation::None);

    // Do cleanup
    cairo_destroy(targetRef);

    return true;
}

void BitmapImage::drawFrameMatchingSourceSize(GraphicsContext& ctxt, const FloatRect& dstRect, const IntSize& srcSize, CompositeOperator compositeOp)
{
    size_t frames = frameCount();
    for (size_t i = 0; i < frames; ++i) {
        auto surface = frameImageAtIndex(i);
        if (!surface)
            continue;

        if (cairo_image_surface_get_height(surface.get()) == static_cast<size_t>(srcSize.height()) && cairo_image_surface_get_width(surface.get()) == static_cast<size_t>(srcSize.width())) {
            size_t currentFrame = m_currentFrame;
            m_currentFrame = i;
            draw(ctxt, dstRect, FloatRect(0.0f, 0.0f, srcSize.width(), srcSize.height()), compositeOp, BlendMode::Normal, DecodingMode::Synchronous, ImageOrientation::None);
            m_currentFrame = currentFrame;
            return;
        }
    }

    // No image of the correct size was found, fallback to drawing the current frame
    FloatSize imageSize = BitmapImage::size();
    draw(ctxt, dstRect, FloatRect(0.0f, 0.0f, imageSize.width(), imageSize.height()), compositeOp, BlendMode::Normal, DecodingMode::Synchronous, ImageOrientation::None);
}

} // namespace WebCore

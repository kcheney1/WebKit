/*
 * Copyright (C) 2015-2022 Apple Inc. All rights reserved.
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
#include "DisplayListItems.h"

#include "DisplayListReplayer.h"
#include "Filter.h"
#include "FontCascade.h"
#include "ImageBuffer.h"
#include "MediaPlayer.h"
#include "SharedBuffer.h"
#include <wtf/text/TextStream.h>

#if USE(CG)
#include "GraphicsContextPlatformPrivateCG.h"
#endif

namespace WebCore {
namespace DisplayList {

// Should match RenderTheme::platformFocusRingWidth()
static const float platformFocusRingWidth = 3;

void Save::apply(GraphicsContext& context) const
{
    context.save();
}

void Restore::apply(GraphicsContext& context) const
{
    context.restore();
}

void Translate::apply(GraphicsContext& context) const
{
    context.translate(m_x, m_y);
}

void Rotate::apply(GraphicsContext& context) const
{
    context.rotate(m_angle);
}

void Scale::apply(GraphicsContext& context) const
{
    context.scale(m_size);
}

void SetCTM::apply(GraphicsContext& context) const
{
    context.setCTM(m_transform);
}

void ConcatenateCTM::apply(GraphicsContext& context) const
{
    context.concatCTM(m_transform);
}

void SetInlineFillColor::apply(GraphicsContext& context) const
{
    context.setFillColor(color());
}

void SetInlineStrokeColor::apply(GraphicsContext& context) const
{
    context.setStrokeColor(color());
}

void SetStrokeThickness::apply(GraphicsContext& context) const
{
    context.setStrokeThickness(m_thickness);
}

SetState::SetState(const GraphicsContextState& state)
    : m_state(state)
{
}

void SetState::apply(GraphicsContext& context)
{
    context.updateState(m_state);
}

void SetLineCap::apply(GraphicsContext& context) const
{
    context.setLineCap(m_lineCap);
}

void SetLineDash::apply(GraphicsContext& context) const
{
    context.setLineDash(m_dashArray, m_dashOffset);
}

void SetLineJoin::apply(GraphicsContext& context) const
{
    context.setLineJoin(m_lineJoin);
}

void SetMiterLimit::apply(GraphicsContext& context) const
{
    context.setMiterLimit(m_miterLimit);
}

void ClearShadow::apply(GraphicsContext& context) const
{
    context.clearShadow();
}

void Clip::apply(GraphicsContext& context) const
{
    context.clip(m_rect);
}

void ClipOut::apply(GraphicsContext& context) const
{
    context.clipOut(m_rect);
}

NO_RETURN_DUE_TO_ASSERT void ClipToImageBuffer::apply(GraphicsContext&) const
{
    ASSERT_NOT_REACHED();
}

void ClipToImageBuffer::apply(GraphicsContext& context, WebCore::ImageBuffer& imageBuffer) const
{
    context.clipToImageBuffer(imageBuffer, m_destinationRect);
}

void ClipOutToPath::apply(GraphicsContext& context) const
{
    context.clipOut(m_path);
}

void ClipPath::apply(GraphicsContext& context) const
{
    context.clipPath(m_path, m_windRule);
}

DrawFilteredImageBuffer::DrawFilteredImageBuffer(std::optional<RenderingResourceIdentifier> sourceImageIdentifier, const FloatRect& sourceImageRect, Filter& filter)
    : m_sourceImageIdentifier(sourceImageIdentifier)
    , m_sourceImageRect(sourceImageRect)
    , m_filter(filter)
{
}

NO_RETURN_DUE_TO_ASSERT void DrawFilteredImageBuffer::apply(GraphicsContext&) const
{
    ASSERT_NOT_REACHED();
}

void DrawFilteredImageBuffer::apply(GraphicsContext& context, ImageBuffer* sourceImage, FilterResults& results)
{
    context.drawFilteredImageBuffer(sourceImage, m_sourceImageRect, m_filter, results);
}

DrawGlyphs::DrawGlyphs(RenderingResourceIdentifier fontIdentifier, Vector<GlyphBufferGlyph, 128>&& glyphs, Vector<GlyphBufferAdvance, 128>&& advances, const FloatRect& bounds, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
    : m_fontIdentifier(fontIdentifier)
    , m_glyphs(WTFMove(glyphs))
    , m_advances(WTFMove(advances))
    , m_bounds(bounds)
    , m_localAnchor(localAnchor)
    , m_smoothingMode(smoothingMode)
{
}

DrawGlyphs::DrawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
    : m_fontIdentifier(font.renderingResourceIdentifier())
    , m_localAnchor(localAnchor)
    , m_smoothingMode(smoothingMode)
{
    m_glyphs.reserveInitialCapacity(count);
    m_advances.reserveInitialCapacity(count);
    for (unsigned i = 0; i < count; ++i) {
        m_glyphs.uncheckedAppend(glyphs[i]);
        m_advances.uncheckedAppend(advances[i]);
    }
    computeBounds(font);
}

void DrawGlyphs::apply(GraphicsContext& context, const Font& font) const
{
    context.drawGlyphs(font, m_glyphs.data(), m_advances.data(), m_glyphs.size(), anchorPoint(), m_smoothingMode);
}

void DrawGlyphs::computeBounds(const Font& font)
{
    // FIXME: This code doesn't actually take the extents of the glyphs into consideration. It assumes that
    // the glyph lies entirely within its [(ascent + descent), advance] rect.
    float ascent = font.fontMetrics().floatAscent();
    float descent = font.fontMetrics().floatDescent();
    FloatPoint current = localAnchor();
    size_t numGlyphs = m_glyphs.size();
    for (size_t i = 0; i < numGlyphs; ++i) {
        GlyphBufferAdvance advance = m_advances[i];
        FloatRect glyphRect = FloatRect(current.x(), current.y() - ascent, width(advance), ascent + descent);
        m_bounds.unite(glyphRect);

        current.move(width(advance), height(advance));
    }
}

NO_RETURN_DUE_TO_ASSERT void DrawImageBuffer::apply(GraphicsContext&) const
{
    ASSERT_NOT_REACHED();
}

void DrawImageBuffer::apply(GraphicsContext& context, WebCore::ImageBuffer& imageBuffer) const
{
    context.drawImageBuffer(imageBuffer, m_destinationRect, m_srcRect, m_options);
}

NO_RETURN_DUE_TO_ASSERT void DrawNativeImage::apply(GraphicsContext&) const
{
    ASSERT_NOT_REACHED();
}

void DrawNativeImage::apply(GraphicsContext& context, NativeImage& image) const
{
    context.drawNativeImage(image, m_imageSize, m_destinationRect, m_srcRect, m_options);
}

void DrawSystemImage::apply(GraphicsContext& context) const
{
    context.drawSystemImage(m_systemImage, m_destinationRect);
}

DrawPattern::DrawPattern(RenderingResourceIdentifier imageIdentifier, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
    : m_imageIdentifier(imageIdentifier)
    , m_destination(destRect)
    , m_tileRect(tileRect)
    , m_patternTransform(patternTransform)
    , m_phase(phase)
    , m_spacing(spacing)
    , m_options(options)
{
}

NO_RETURN_DUE_TO_ASSERT void DrawPattern::apply(GraphicsContext&) const
{
    ASSERT_NOT_REACHED();
}

void DrawPattern::apply(GraphicsContext& context, SourceImage& sourceImage) const
{
    if (auto image = sourceImage.nativeImageIfExists()) {
        context.drawPattern(*image, m_destination, m_tileRect, m_patternTransform, m_phase, m_spacing, m_options);
        return;
    }

    if (auto imageBuffer = sourceImage.imageBufferIfExists()) {
        context.drawPattern(*imageBuffer, m_destination, m_tileRect, m_patternTransform, m_phase, m_spacing, m_options);
        return;
    }

    ASSERT_NOT_REACHED();
}

void DrawRect::apply(GraphicsContext& context) const
{
    context.drawRect(m_rect, m_borderThickness);
}

std::optional<FloatRect> DrawLine::localBounds(const GraphicsContext&) const
{
    FloatRect bounds;
    bounds.fitToPoints(m_point1, m_point2);
    return bounds;
}

void DrawLine::apply(GraphicsContext& context) const
{
    context.drawLine(m_point1, m_point2);
}

DrawLinesForText::DrawLinesForText(const FloatPoint& blockLocation, const FloatSize& localAnchor, float thickness, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle style)
    : m_blockLocation(blockLocation)
    , m_localAnchor(localAnchor)
    , m_widths(widths)
    , m_thickness(thickness)
    , m_printing(printing)
    , m_doubleLines(doubleLines)
    , m_style(style)
{
}

void DrawLinesForText::apply(GraphicsContext& context) const
{
    context.drawLinesForText(point(), m_thickness, m_widths, m_printing, m_doubleLines, m_style);
}

std::optional<FloatRect> DrawLinesForText::localBounds(const GraphicsContext&) const
{
    // This function needs to return a value equal to or enclosing what GraphicsContext::computeLineBoundsAndAntialiasingModeForText() returns.

    if (!m_widths.size())
        return FloatRect();

    FloatRect result(point(), FloatSize(m_widths.last(), m_thickness));
    result.inflate(1); // Account for pixel snapping. FIXME: This isn't perfect, as it doesn't take the CTM into account.
    return result;
}

void DrawDotsForDocumentMarker::apply(GraphicsContext& context) const
{
    context.drawDotsForDocumentMarker(m_rect, {
        static_cast<DocumentMarkerLineStyle::Mode>(m_styleMode),
        m_styleShouldUseDarkAppearance,
    });
}

std::optional<FloatRect> DrawDotsForDocumentMarker::localBounds(const GraphicsContext&) const
{
    return m_rect;
}

void DrawEllipse::apply(GraphicsContext& context) const
{
    context.drawEllipse(m_rect);
}

void DrawPath::apply(GraphicsContext& context) const
{
    context.drawPath(m_path);
}

void DrawFocusRingPath::apply(GraphicsContext& context) const
{
    context.drawFocusRing(m_path, m_width, m_offset, m_color);
}

std::optional<FloatRect> DrawFocusRingPath::localBounds(const GraphicsContext&) const
{
    FloatRect result = m_path.fastBoundingRect();
    result.inflate(platformFocusRingWidth);
    return result;
}

DrawFocusRingRects::DrawFocusRingRects(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
    : m_rects(rects)
    , m_width(width)
    , m_offset(offset)
    , m_color(color)
{
}

void DrawFocusRingRects::apply(GraphicsContext& context) const
{
    context.drawFocusRing(m_rects, m_width, m_offset, m_color);
}

std::optional<FloatRect> DrawFocusRingRects::localBounds(const GraphicsContext&) const
{
    FloatRect result;
    for (auto& rect : m_rects)
        result.unite(rect);
    result.inflate(platformFocusRingWidth);
    return result;
}

void FillRect::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect);
}

void FillRectWithColor::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect, m_color);
}

FillRectWithGradient::FillRectWithGradient(const FloatRect& rect, Gradient& gradient)
    : m_rect(rect)
    , m_gradient(gradient)
{
}

void FillRectWithGradient::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect, m_gradient.get());
}

void FillCompositedRect::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect, m_color, m_op, m_blendMode);
}

void FillRoundedRect::apply(GraphicsContext& context) const
{
    context.fillRoundedRect(m_rect, m_color, m_blendMode);
}

void FillRectWithRoundedHole::apply(GraphicsContext& context) const
{
    context.fillRectWithRoundedHole(m_rect, m_roundedHoleRect, m_color);
}

#if ENABLE(INLINE_PATH_DATA)

void FillLine::apply(GraphicsContext& context) const
{
    context.fillPath(path());
}

void FillArc::apply(GraphicsContext& context) const
{
    context.fillPath(path());
}

void FillQuadCurve::apply(GraphicsContext& context) const
{
    context.fillPath(path());
}

void FillBezierCurve::apply(GraphicsContext& context) const
{
    context.fillPath(path());
}

#endif // ENABLE(INLINE_PATH_DATA)

void FillPath::apply(GraphicsContext& context) const
{
    context.fillPath(m_path);
}

void FillEllipse::apply(GraphicsContext& context) const
{
    context.fillEllipse(m_rect);
}

#if ENABLE(VIDEO)
PaintFrameForMedia::PaintFrameForMedia(MediaPlayer& player, const FloatRect& destination)
    : m_identifier(player.identifier())
    , m_destination(destination)
{
}

NO_RETURN_DUE_TO_ASSERT void PaintFrameForMedia::apply(GraphicsContext&) const
{
    // Should be handled by the delegate.
    ASSERT_NOT_REACHED();
}

#endif

std::optional<FloatRect> StrokeRect::localBounds(const GraphicsContext&) const
{
    FloatRect bounds = m_rect;
    bounds.expand(m_lineWidth, m_lineWidth);
    return bounds;
}

void StrokeRect::apply(GraphicsContext& context) const
{
    context.strokeRect(m_rect, m_lineWidth);
}

std::optional<FloatRect> StrokePath::localBounds(const GraphicsContext& context) const
{
    // FIXME: Need to take stroke thickness into account correctly, via CGPathByStrokingPath().
    float strokeThickness = context.strokeThickness();

    FloatRect bounds = m_path.fastBoundingRect();
    bounds.expand(strokeThickness, strokeThickness);
    return bounds;
}

void StrokePath::apply(GraphicsContext& context) const
{
    context.strokePath(m_path);
}

std::optional<FloatRect> StrokeEllipse::localBounds(const GraphicsContext& context) const
{
    float strokeThickness = context.strokeThickness();

    FloatRect bounds = m_rect;
    bounds.expand(strokeThickness, strokeThickness);
    return bounds;
}

void StrokeEllipse::apply(GraphicsContext& context) const
{
    context.strokeEllipse(m_rect);
}

std::optional<FloatRect> StrokeLine::localBounds(const GraphicsContext& context) const
{
    float strokeThickness = context.strokeThickness();

    FloatRect bounds;
    bounds.fitToPoints(start(), end());
    bounds.expand(strokeThickness, strokeThickness);
    return bounds;
}

void StrokeLine::apply(GraphicsContext& context) const
{
#if ENABLE(INLINE_PATH_DATA)
    auto path = Path::from(InlinePathData { LineData { start(), end() } });
#else
    Path path;
    path.moveTo(start());
    path.addLineTo(end());
#endif
    context.strokePath(path);
}

#if ENABLE(INLINE_PATH_DATA)

std::optional<FloatRect> StrokeArc::localBounds(const GraphicsContext& context) const
{
    // FIXME: Need to take stroke thickness into account correctly, via CGPathByStrokingPath().
    float strokeThickness = context.strokeThickness();

    auto bounds = path().fastBoundingRect();
    bounds.expand(strokeThickness, strokeThickness);
    return bounds;
}

void StrokeArc::apply(GraphicsContext& context) const
{
    context.strokePath(path());
}

std::optional<FloatRect> StrokeQuadCurve::localBounds(const GraphicsContext& context) const
{
    // FIXME: Need to take stroke thickness into account correctly, via CGPathByStrokingPath().
    float strokeThickness = context.strokeThickness();

    auto bounds = path().fastBoundingRect();
    bounds.expand(strokeThickness, strokeThickness);
    return bounds;
}

void StrokeQuadCurve::apply(GraphicsContext& context) const
{
    context.strokePath(path());
}

std::optional<FloatRect> StrokeBezierCurve::localBounds(const GraphicsContext& context) const
{
    // FIXME: Need to take stroke thickness into account correctly, via CGPathByStrokingPath().
    float strokeThickness = context.strokeThickness();

    auto bounds = path().fastBoundingRect();
    bounds.expand(strokeThickness, strokeThickness);
    return bounds;
}

void StrokeBezierCurve::apply(GraphicsContext& context) const
{
    context.strokePath(path());
}

#endif // ENABLE(INLINE_PATH_DATA)

void ClearRect::apply(GraphicsContext& context) const
{
    context.clearRect(m_rect);
}

void BeginTransparencyLayer::apply(GraphicsContext& context) const
{
    context.beginTransparencyLayer(m_opacity);
}

void EndTransparencyLayer::apply(GraphicsContext& context) const
{
    if (context.isInTransparencyLayer())
        context.endTransparencyLayer();
}

#if USE(CG)

void ApplyStrokePattern::apply(GraphicsContext& context) const
{
    context.applyStrokePattern();
}

void ApplyFillPattern::apply(GraphicsContext& context) const
{
    context.applyFillPattern();
}
#endif

void ApplyDeviceScaleFactor::apply(GraphicsContext& context) const
{
    context.applyDeviceScaleFactor(m_scaleFactor);
}

#if !LOG_DISABLED
static TextStream& operator<<(TextStream& ts, ItemType type)
{
    switch (type) {
    case ItemType::Save: ts << "save"; break;
    case ItemType::Restore: ts << "restore"; break;
    case ItemType::Translate: ts << "translate"; break;
    case ItemType::Rotate: ts << "rotate"; break;
    case ItemType::Scale: ts << "scale"; break;
    case ItemType::SetCTM: ts << "set-ctm"; break;
    case ItemType::ConcatenateCTM: ts << "concatentate-ctm"; break;
    case ItemType::SetInlineFillColor: ts << "set-inline-fill-color"; break;
    case ItemType::SetInlineStrokeColor: ts << "set-inline-stroke-color"; break;
    case ItemType::SetStrokeThickness: ts << "set-stroke-thickness"; break;
    case ItemType::SetState: ts << "set-state"; break;
    case ItemType::SetLineCap: ts << "set-line-cap"; break;
    case ItemType::SetLineDash: ts << "set-line-dash"; break;
    case ItemType::SetLineJoin: ts << "set-line-join"; break;
    case ItemType::SetMiterLimit: ts << "set-miter-limit"; break;
    case ItemType::Clip: ts << "clip"; break;
    case ItemType::ClipOut: ts << "clip-out"; break;
    case ItemType::ClipToImageBuffer: ts << "clip-to-image-buffer"; break;
    case ItemType::ClipOutToPath: ts << "clip-out-to-path"; break;
    case ItemType::ClipPath: ts << "clip-path"; break;
    case ItemType::DrawFilteredImageBuffer: ts << "draw-filtered-image-buffer"; break;
    case ItemType::DrawGlyphs: ts << "draw-glyphs"; break;
    case ItemType::DrawImageBuffer: ts << "draw-image-buffer"; break;
    case ItemType::DrawNativeImage: ts << "draw-native-image"; break;
    case ItemType::DrawSystemImage: ts << "draw-system-image"; break;
    case ItemType::DrawPattern: ts << "draw-pattern"; break;
    case ItemType::DrawRect: ts << "draw-rect"; break;
    case ItemType::DrawLine: ts << "draw-line"; break;
    case ItemType::DrawLinesForText: ts << "draw-lines-for-text"; break;
    case ItemType::DrawDotsForDocumentMarker: ts << "draw-dots-for-document-marker"; break;
    case ItemType::DrawEllipse: ts << "draw-ellipse"; break;
    case ItemType::DrawPath: ts << "draw-path"; break;
    case ItemType::DrawFocusRingPath: ts << "draw-focus-ring-path"; break;
    case ItemType::DrawFocusRingRects: ts << "draw-focus-ring-rects"; break;
    case ItemType::FillRect: ts << "fill-rect"; break;
    case ItemType::FillRectWithColor: ts << "fill-rect-with-color"; break;
    case ItemType::FillRectWithGradient: ts << "fill-rect-with-gradient"; break;
    case ItemType::FillCompositedRect: ts << "fill-composited-rect"; break;
    case ItemType::FillRoundedRect: ts << "fill-rounded-rect"; break;
    case ItemType::FillRectWithRoundedHole: ts << "fill-rect-with-rounded-hole"; break;
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::FillLine: ts << "fill-line"; break;
    case ItemType::FillArc: ts << "fill-arc"; break;
    case ItemType::FillQuadCurve: ts << "fill-quad-curve"; break;
    case ItemType::FillBezierCurve: ts << "fill-bezier-curve"; break;
#endif
    case ItemType::FillPath: ts << "fill-path"; break;
    case ItemType::FillEllipse: ts << "fill-ellipse"; break;
#if ENABLE(VIDEO)
    case ItemType::PaintFrameForMedia: ts << "paint-frame-for-media"; break;
#endif
    case ItemType::StrokeRect: ts << "stroke-rect"; break;
    case ItemType::StrokeLine: ts << "stroke-line"; break;
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::StrokeArc: ts << "stroke-arc"; break;
    case ItemType::StrokeQuadCurve: ts << "stroke-quad-curve"; break;
    case ItemType::StrokeBezierCurve: ts << "stroke-bezier-curve"; break;
#endif
    case ItemType::StrokePath: ts << "stroke-path"; break;
    case ItemType::StrokeEllipse: ts << "stroke-ellipse"; break;
    case ItemType::ClearRect: ts << "clear-rect"; break;
    case ItemType::BeginTransparencyLayer: ts << "begin-transparency-layer"; break;
    case ItemType::EndTransparencyLayer: ts << "end-transparency-layer"; break;
#if USE(CG)
    case ItemType::ApplyStrokePattern: ts << "apply-stroke-pattern"; break;
    case ItemType::ApplyFillPattern: ts << "apply-fill-pattern"; break;
#endif
    case ItemType::ApplyDeviceScaleFactor: ts << "apply-device-scale-factor"; break;
    case ItemType::ClearShadow: ts << "clear-shadow"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const Translate& item)
{
    ts.dumpProperty("x", item.x());
    ts.dumpProperty("y", item.y());
    return ts;
}

TextStream& operator<<(TextStream& ts, const Rotate& item)
{
    ts.dumpProperty("angle", item.angle());
    return ts;
}

TextStream& operator<<(TextStream& ts, const Scale& item)
{
    ts.dumpProperty("size", item.amount());
    return ts;
}

TextStream& operator<<(TextStream& ts, const SetCTM& item)
{
    ts.dumpProperty("set-ctm", item.transform());
    return ts;
}

TextStream& operator<<(TextStream& ts, const ConcatenateCTM& item)
{
    ts.dumpProperty("ctm", item.transform());
    return ts;
}

TextStream& operator<<(TextStream& ts, const SetInlineFillColor& state)
{
    ts.dumpProperty("color", state.color());
    return ts;
}

TextStream& operator<<(TextStream& ts, const SetInlineStrokeColor& state)
{
    ts.dumpProperty("color", state.color());
    return ts;
}

TextStream& operator<<(TextStream& ts, const SetStrokeThickness& state)
{
    ts.dumpProperty("thickness", state.thickness());
    return ts;
}

TextStream& operator<<(TextStream& ts, const SetState& item)
{
    ts << item.state();
    return ts;
}

TextStream& operator<<(TextStream& ts, const SetLineCap& lineCap)
{
    ts.dumpProperty("line-cap", lineCap.lineCap());
    return ts;
}

TextStream& operator<<(TextStream& ts, const SetLineDash& lineDash)
{
    ts.dumpProperty("dash-array", lineDash.dashArray());
    ts.dumpProperty("dash-offset", lineDash.dashOffset());
    return ts;
}

TextStream& operator<<(TextStream& ts, const SetLineJoin& lineJoin)
{
    ts.dumpProperty("line-join", lineJoin.lineJoin());
    return ts;
}

TextStream& operator<<(TextStream& ts, const SetMiterLimit& miterLimit)
{
    ts.dumpProperty("mitre-limit", miterLimit.miterLimit());
    return ts;
}

TextStream& operator<<(TextStream& ts, const Clip& item)
{
    ts.dumpProperty("rect", item.rect());
    return ts;
}

TextStream& operator<<(TextStream& ts, const ClipOut& item)
{
    ts.dumpProperty("rect", item.rect());
    return ts;
}

TextStream& operator<<(TextStream& ts, const ClipToImageBuffer& item)
{
    ts.dumpProperty("image-buffer-identifier", item.imageBufferIdentifier());
    ts.dumpProperty("dest-rect", item.destinationRect());
    return ts;
}

TextStream& operator<<(TextStream& ts, const ClipOutToPath& item)
{
    ts.dumpProperty("path", item.path());
    return ts;
}

TextStream& operator<<(TextStream& ts, const ClipPath& item)
{
    ts.dumpProperty("path", item.path());
    ts.dumpProperty("wind-rule", item.windRule());
    return ts;
}

TextStream& operator<<(TextStream& ts, const DrawFilteredImageBuffer& item)
{
    ts.dumpProperty("source-image-identifier", item.sourceImageIdentifier());
    ts.dumpProperty("source-image-rect", item.sourceImageRect());
    return ts;
}

TextStream& operator<<(TextStream& ts, const DrawGlyphs& item)
{
    // FIXME: dump more stuff.
    ts.dumpProperty("local-anchor", item.localAnchor());
    ts.dumpProperty("anchor-point", item.anchorPoint());
    ts.dumpProperty("length", item.glyphs().size());

    return ts;
}

TextStream& operator<<(TextStream& ts, const DrawImageBuffer& item)
{
    ts.dumpProperty("image-buffer-identifier", item.imageBufferIdentifier());
    ts.dumpProperty("source-rect", item.source());
    ts.dumpProperty("dest-rect", item.destinationRect());
    return ts;
}

TextStream& operator<<(TextStream& ts, const DrawNativeImage& item)
{
    ts.dumpProperty("image-identifier", item.imageIdentifier());
    ts.dumpProperty("source-rect", item.source());
    ts.dumpProperty("dest-rect", item.destinationRect());
    return ts;
}

TextStream& operator<<(TextStream& ts, const DrawSystemImage& item)
{
    // FIXME: dump more stuff.
    ts.dumpProperty("destination", item.destinationRect());
    return ts;
}

TextStream& operator<<(TextStream& ts, const DrawPattern& item)
{
    ts.dumpProperty("image-identifier", item.imageIdentifier());
    ts.dumpProperty("pattern-transform", item.patternTransform());
    ts.dumpProperty("tile-rect", item.tileRect());
    ts.dumpProperty("dest-rect", item.destRect());
    ts.dumpProperty("phase", item.phase());
    ts.dumpProperty("spacing", item.spacing());
    return ts;
}

TextStream& operator<<(TextStream& ts, const DrawRect& item)
{
    ts.dumpProperty("rect", item.rect());
    ts.dumpProperty("border-thickness", item.borderThickness());
    return ts;
}

TextStream& operator<<(TextStream& ts, const DrawLine& item)
{
    ts.dumpProperty("point-1", item.point1());
    ts.dumpProperty("point-2", item.point2());
    return ts;
}

TextStream& operator<<(TextStream& ts, const DrawLinesForText& item)
{
    ts.dumpProperty("block-location", item.blockLocation());
    ts.dumpProperty("local-anchor", item.localAnchor());
    ts.dumpProperty("point", item.point());
    ts.dumpProperty("thickness", item.thickness());
    ts.dumpProperty("double", item.doubleLines());
    ts.dumpProperty("widths", item.widths());
    ts.dumpProperty("is-printing", item.isPrinting());
    ts.dumpProperty("double", item.doubleLines());
    return ts;
}

TextStream& operator<<(TextStream& ts, const DrawDotsForDocumentMarker& item)
{
    ts.dumpProperty("rect", item.rect());
    return ts;
}

TextStream& operator<<(TextStream& ts, const DrawEllipse& item)
{
    ts.dumpProperty("rect", item.rect());
    return ts;
}

TextStream& operator<<(TextStream& ts, const DrawPath& item)
{
    ts.dumpProperty("path", item.path());
    return ts;
}

TextStream& operator<<(TextStream& ts, const DrawFocusRingPath& item)
{
    ts.dumpProperty("path", item.path());
    ts.dumpProperty("width", item.width());
    ts.dumpProperty("offset", item.offset());
    ts.dumpProperty("color", item.color());
    return ts;
}

TextStream& operator<<(TextStream& ts, const DrawFocusRingRects& item)
{
    ts.dumpProperty("rects", item.rects());
    ts.dumpProperty("width", item.width());
    ts.dumpProperty("offset", item.offset());
    ts.dumpProperty("color", item.color());
    return ts;
}

TextStream& operator<<(TextStream& ts, const FillRect& item)
{
    ts.dumpProperty("rect", item.rect());
    return ts;
}

TextStream& operator<<(TextStream& ts, const FillRectWithColor& item)
{
    ts.dumpProperty("rect", item.rect());
    ts.dumpProperty("color", item.color());
    return ts;
}

TextStream& operator<<(TextStream& ts, const FillRectWithGradient& item)
{
    // FIXME: log gradient.
    ts.dumpProperty("rect", item.rect());
    return ts;
}

TextStream& operator<<(TextStream& ts, const FillCompositedRect& item)
{
    ts.dumpProperty("rect", item.rect());
    ts.dumpProperty("color", item.color());
    ts.dumpProperty("composite-operation", item.compositeOperator());
    ts.dumpProperty("blend-mode", item.blendMode());
    return ts;
}

TextStream& operator<<(TextStream& ts, const FillRoundedRect& item)
{
    ts.dumpProperty("rect", item.roundedRect());
    ts.dumpProperty("color", item.color());
    ts.dumpProperty("blend-mode", item.blendMode());
    return ts;
}

TextStream& operator<<(TextStream& ts, const FillRectWithRoundedHole& item)
{
    ts.dumpProperty("rect", item.rect());
    ts.dumpProperty("rounded-hole-rect", item.roundedHoleRect());
    ts.dumpProperty("color", item.color());
    return ts;
}

#if ENABLE(INLINE_PATH_DATA)

TextStream& operator<<(TextStream& ts, const FillLine& item)
{
    ts.dumpProperty("path", item.path());
    return ts;
}

TextStream& operator<<(TextStream& ts, const FillArc& item)
{
    ts.dumpProperty("path", item.path());
    return ts;
}

TextStream& operator<<(TextStream& ts, const FillQuadCurve& item)
{
    ts.dumpProperty("path", item.path());
    return ts;
}

TextStream& operator<<(TextStream& ts, const FillBezierCurve& item)
{
    ts.dumpProperty("path", item.path());
    return ts;
}

TextStream& operator<<(TextStream& ts, const StrokeArc& item)
{
    ts.dumpProperty("path", item.path());
    return ts;
}

TextStream& operator<<(TextStream& ts, const StrokeQuadCurve& item)
{
    ts.dumpProperty("path", item.path());
    return ts;
}

TextStream& operator<<(TextStream& ts, const StrokeBezierCurve& item)
{
    ts.dumpProperty("path", item.path());
    return ts;
}

#endif // ENABLE(INLINE_PATH_DATA)

TextStream& operator<<(TextStream& ts, const FillPath& item)
{
    ts.dumpProperty("path", item.path());
    return ts;
}

TextStream& operator<<(TextStream& ts, const FillEllipse& item)
{
    ts.dumpProperty("rect", item.rect());
    return ts;
}

#if ENABLE(VIDEO)

TextStream& operator<<(TextStream& ts, const PaintFrameForMedia& item)
{
    ts.dumpProperty("destination", item.destination());
    return ts;
}

#endif // ENABLE(VIDEO)


TextStream& operator<<(TextStream& ts, const StrokeRect& item)
{
    ts.dumpProperty("rect", item.rect());
    ts.dumpProperty("line-width", item.lineWidth());
    return ts;
}

TextStream& operator<<(TextStream& ts, const StrokePath& item)
{
    ts.dumpProperty("path", item.path());
    return ts;
}

TextStream& operator<<(TextStream& ts, const StrokeEllipse& item)
{
    ts.dumpProperty("rect", item.rect());
    return ts;
}

TextStream& operator<<(TextStream& ts, const StrokeLine& item)
{
    ts.dumpProperty("start", item.start());
    ts.dumpProperty("end", item.end());
    return ts;
}

TextStream& operator<<(TextStream& ts, const ClearRect& item)
{
    ts.dumpProperty("rect", item.rect());
    return ts;
}

TextStream& operator<<(TextStream& ts, const BeginTransparencyLayer& item)
{
    ts.dumpProperty("opacity", item.opacity());
    return ts;
}

TextStream& operator<<(TextStream& ts, const ApplyDeviceScaleFactor& item)
{
    ts.dumpProperty("scale-factor", item.scaleFactor());
    return ts;
}

TextStream& operator<<(TextStream& ts, ItemHandle item)
{
    ts << item.type();

    switch (item.type()) {
    case ItemType::Translate:
        ts << item.get<Translate>();
        break;
    case ItemType::Rotate:
        ts << item.get<Rotate>();
        break;
    case ItemType::Scale:
        ts << item.get<Scale>();
        break;
    case ItemType::SetCTM:
        ts << item.get<SetCTM>();
        break;
    case ItemType::ConcatenateCTM:
        ts << item.get<ConcatenateCTM>();
        break;
    case ItemType::SetInlineFillColor:
        ts << item.get<SetInlineFillColor>();
        break;
    case ItemType::SetInlineStrokeColor:
        ts << item.get<SetInlineStrokeColor>();
        break;
    case ItemType::SetStrokeThickness:
        ts << item.get<SetStrokeThickness>();
        break;
    case ItemType::SetState:
        ts << item.get<SetState>();
        break;
    case ItemType::SetLineCap:
        ts << item.get<SetLineCap>();
        break;
    case ItemType::SetLineDash:
        ts << item.get<SetLineDash>();
        break;
    case ItemType::SetLineJoin:
        ts << item.get<SetLineJoin>();
        break;
    case ItemType::SetMiterLimit:
        ts << item.get<SetMiterLimit>();
        break;
    case ItemType::Clip:
        ts << item.get<Clip>();
        break;
    case ItemType::ClipOut:
        ts << item.get<ClipOut>();
        break;
    case ItemType::ClipToImageBuffer:
        ts << item.get<ClipToImageBuffer>();
        break;
    case ItemType::ClipOutToPath:
        ts << item.get<ClipOutToPath>();
        break;
    case ItemType::ClipPath:
        ts << item.get<ClipPath>();
        break;
    case ItemType::DrawFilteredImageBuffer:
        ts << item.get<DrawFilteredImageBuffer>();
        break;
    case ItemType::DrawGlyphs:
        ts << item.get<DrawGlyphs>();
        break;
    case ItemType::DrawImageBuffer:
        ts << item.get<DrawImageBuffer>();
        break;
    case ItemType::DrawNativeImage:
        ts << item.get<DrawNativeImage>();
        break;
    case ItemType::DrawSystemImage:
        ts << item.get<DrawSystemImage>();
        break;
    case ItemType::DrawPattern:
        ts << item.get<DrawPattern>();
        break;
    case ItemType::DrawRect:
        ts << item.get<DrawRect>();
        break;
    case ItemType::DrawLine:
        ts << item.get<DrawLine>();
        break;
    case ItemType::DrawLinesForText:
        ts << item.get<DrawLinesForText>();
        break;
    case ItemType::DrawDotsForDocumentMarker:
        ts << item.get<DrawDotsForDocumentMarker>();
        break;
    case ItemType::DrawEllipse:
        ts << item.get<DrawEllipse>();
        break;
    case ItemType::DrawPath:
        ts << item.get<DrawPath>();
        break;
    case ItemType::DrawFocusRingPath:
        ts << item.get<DrawFocusRingPath>();
        break;
    case ItemType::DrawFocusRingRects:
        ts << item.get<DrawFocusRingRects>();
        break;
    case ItemType::FillRect:
        ts << item.get<FillRect>();
        break;
    case ItemType::FillRectWithColor:
        ts << item.get<FillRectWithColor>();
        break;
    case ItemType::FillRectWithGradient:
        ts << item.get<FillRectWithGradient>();
        break;
    case ItemType::FillCompositedRect:
        ts << item.get<FillCompositedRect>();
        break;
    case ItemType::FillRoundedRect:
        ts << item.get<FillRoundedRect>();
        break;
    case ItemType::FillRectWithRoundedHole:
        ts << item.get<FillRectWithRoundedHole>();
        break;
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::FillLine:
        ts << item.get<FillLine>();
        break;
    case ItemType::FillArc:
        ts << item.get<FillArc>();
        break;
    case ItemType::FillQuadCurve:
        ts << item.get<FillQuadCurve>();
        break;
    case ItemType::FillBezierCurve:
        ts << item.get<FillBezierCurve>();
        break;
#endif
    case ItemType::FillPath:
        ts << item.get<FillPath>();
        break;
    case ItemType::FillEllipse:
        ts << item.get<FillEllipse>();
        break;
#if ENABLE(VIDEO)
    case ItemType::PaintFrameForMedia:
        ts << item.get<PaintFrameForMedia>();
        break;
#endif
    case ItemType::StrokeRect:
        ts << item.get<StrokeRect>();
        break;
    case ItemType::StrokeLine:
        ts << item.get<StrokeLine>();
        break;
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::StrokeArc:
        ts << item.get<StrokeArc>();
        break;
    case ItemType::StrokeQuadCurve:
        ts << item.get<StrokeQuadCurve>();
        break;
    case ItemType::StrokeBezierCurve:
        ts << item.get<StrokeBezierCurve>();
        break;
#endif
    case ItemType::StrokePath:
        ts << item.get<StrokePath>();
        break;
    case ItemType::StrokeEllipse:
        ts << item.get<StrokeEllipse>();
        break;
    case ItemType::ClearRect:
        ts << item.get<ClearRect>();
        break;
    case ItemType::BeginTransparencyLayer:
        ts << item.get<BeginTransparencyLayer>();
        break;
    case ItemType::ApplyDeviceScaleFactor:
        ts << item.get<ApplyDeviceScaleFactor>();
        break;

    // Items with no additional data.
    case ItemType::Save:
    case ItemType::Restore:
    case ItemType::EndTransparencyLayer:
#if USE(CG)
    case ItemType::ApplyStrokePattern:
    case ItemType::ApplyFillPattern:
#endif
    case ItemType::ClearShadow:
        break;
    }
    return ts;
}
#endif

} // namespace DisplayList
} // namespace WebCore

/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PathOperation.h"

#include "GeometryUtilities.h"
#include "SVGElement.h"

namespace WebCore {

Ref<ReferencePathOperation> ReferencePathOperation::create(const String& url, const AtomString& fragment, const RefPtr<SVGElement> element)
{
    return adoptRef(*new ReferencePathOperation(url, fragment, element));
}


ReferencePathOperation::ReferencePathOperation(const String& url, const AtomString& fragment, const RefPtr<SVGElement> element)
    : PathOperation(Reference)
    , m_url(url)
    , m_fragment(fragment)
    , m_element(element)
{
}

const SVGElement* ReferencePathOperation::element() const
{
    return m_element.get();
}

double RayPathOperation::lengthForPath() const
{
    auto boundingBox = m_containingBlockBoundingRect;
    auto distances = distanceOfPointToSidesOfRect(boundingBox, m_position);
    
    switch (m_size) {
    case Size::ClosestSide:
        return std::min( { distances.top(), distances.bottom(), distances.left(), distances.right() } );
    case Size::FarthestSide:
        return std::max( { distances.top(), distances.bottom(), distances.left(), distances.right() } );
    case Size::FarthestCorner:
        return std::sqrt(std::pow(std::max(distances.left(), distances.right()), 2) + std::pow(std::max(distances.top(), distances.bottom()), 2));
    case Size::ClosestCorner:
        return std::sqrt(std::pow(std::min(distances.left(), distances.right()), 2) + std::pow(std::min(distances.top(), distances.bottom()), 2));
    case Size::Sides:
        return lengthOfRayIntersectionWithBoundingBox(boundingBox, std::make_pair(m_position, m_angle));
    }
}

const Path RayPathOperation::pathForReferenceRect() const
{
    Path path;
    if (m_containingBlockBoundingRect.isZero())
        return path;
    auto length = lengthForPath();
    auto radians = deg2rad(toPositiveAngle(m_angle) - 90);
    auto point = FloatPoint(std::cos(radians) * length, std::sin(radians) * length);
    path.addLineTo(point);
    return path;
}

} // namespace WebCore

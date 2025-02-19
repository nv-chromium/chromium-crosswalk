/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef NavigatorMediaStream_h
#define NavigatorMediaStream_h

#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Dictionary;
class ExceptionState;
class MediaDeviceInfoCallback;
class MediaStreamConstraints;
class Navigator;
class NavigatorUserMediaErrorCallback;
class NavigatorUserMediaSuccessCallback;

class NavigatorMediaStream {
public:
    static void webkitGetUserMedia(Navigator&, const MediaStreamConstraints&, NavigatorUserMediaSuccessCallback*, NavigatorUserMediaErrorCallback*, ExceptionState&);

    static void getMediaDevices(Navigator&, MediaDeviceInfoCallback*, ExceptionState&);

private:
    NavigatorMediaStream();
    ~NavigatorMediaStream();
};

} // namespace blink

#endif // NavigatorMediaStream_h

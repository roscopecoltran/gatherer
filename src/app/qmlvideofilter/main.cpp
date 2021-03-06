/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2015 Ruslan Baratov
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Multimedia module.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <cassert> // assert

#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QCamera>
#include <QQuickWindow>
#include <QCameraInfo>
#include <QCameraImageCapture>
#include <QMediaRecorder>
#include <QtPlugin> // Q_IMPORT_PLUGIN
#include <QQmlExtensionPlugin>
#include <QtOpenGL/QGLFormat>

#include "VideoFilterRunnable.hpp"
#include "VideoFilter.hpp"
#include "InfoFilter.hpp"
#include "QTRenderGL.hpp"

#include "FrameHandler.h"

#include "graphics/Logger.h"

#define TEST_CALLBACK 0
#if TEST_CALLBACK
#  include "OGLESGPGPUTest.h"
#  include <opencv2/highgui.hpp>
#endif

#include <iostream>

#if defined(Q_OS_OSX)
Q_IMPORT_PLUGIN(QtQuick2Plugin);
Q_IMPORT_PLUGIN(QMultimediaDeclarativeModule);
#endif

#if defined(Q_OS_IOS)
extern "C" int qtmn(int argc, char** argv)
{
#else
int main(int argc, char **argv)
{
#endif
#ifdef Q_OS_WIN // avoid ANGLE on Windows
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
#endif
    
    QGuiApplication app(argc, argv);

    auto logger = gatherer::graphics::Logger::create("qmlvideofilter");
    logger->info() << "Start";

    qmlRegisterType<VideoFilter>("qmlvideofilter.test", 1, 0, "VideoFilter");
    qmlRegisterType<InfoFilter>("qmlvideofilter.test", 1, 0, "InfoFilter");
    qmlRegisterType<QTRenderGL>("OpenGLUnderQML", 1, 0, "QTRenderGL");

#if defined(Q_OS_OSX)
    qobject_cast<QQmlExtensionPlugin*>(qt_static_plugin_QtQuick2Plugin().instance())->registerTypes("QtQuick");
    qobject_cast<QQmlExtensionPlugin*>(qt_static_plugin_QMultimediaDeclarativeModule().instance())->registerTypes("QtMultimedia");
#endif
    
    QQuickView view;
    view.setSource(QUrl("qrc:///main.qml"));
    view.setResizeMode( QQuickView::SizeRootObjectToView );
    
#if defined(Q_OS_OSX)
    
    // This had been tested with GLFW + ogles_gpgpu before
    //OpenGL version: 2.1 NVIDIA-10.4.2 310.41.35f01
    //GLSL version: 1.20
    
    // Currently texture2D() calls in our shaders are failing on OS X builds.
    // and the output texture appears black.  The input texture displays fine
    // in the QML VideoOutput and if we write directly to the gl_FragColor from
    // the same shader (ignoring the input texture) then the output texture
    // displays properly in QML VideoOutput as well.  This would seem to point
    // to a deprecated texture2D() behavior which could be remedied by setting
    // the version/profile (several configurations tested without luck so far).
    
    QSurfaceFormat format;
    format.setVersion(2, 1);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    view.setFormat(format);
    
    logger->info() << "OpenGL Versions Supported: " << QGLFormat::openGLVersionFlags();
#endif
    
    // Default camera on iOS is not setting good parameters by default
    QQuickItem* root = view.rootObject();
    
    QObject* qmlCamera = root->findChild<QObject*>("CameraObject");
    assert(qmlCamera != nullptr);
    
    QCamera* camera = qvariant_cast<QCamera*>(qmlCamera->property("mediaObject"));
    assert(camera != nullptr);
    
    QObject * qmlVideoOutput = root->findChild<QObject*>("VideoOutput");
    assert(qmlVideoOutput);

    {
        // Not available in Android:
        // https://bugreports.qt.io/browse/QTBUG-46470
        
        // Try the highest resolution NV{12,21} format format:
        // This should work for both Android and iOS
        std::vector<QVideoFrame::PixelFormat> desiredFormats;
        
#if defined(Q_OS_IOS)
        desiredFormats = { QVideoFrame::Format_NV12, QVideoFrame::Format_NV21 };
#elif defined(Q_OS_ANDROID)
        desiredFormats = { QVideoFrame::Format_NV12, QVideoFrame::Format_NV21, QVideoFrame::Format_YV12 };
#else
        // TODO: add OS X texture cache for Y + UV texture loads
        desiredFormats = { QVideoFrame::Format_ARGB32 };
#endif
        auto viewfinderSettings = camera->supportedViewfinderSettings();
        
        logger->info() << "# of settings: " << viewfinderSettings.size();
        
        std::pair<int, QCameraViewfinderSettings> best;
        for (auto i: viewfinderSettings)
        {
            logger->info() << "settings: " << i.resolution().width() << "x" << i.resolution().height() << " : " << int(i.pixelFormat())
                << " (fps from " << i.minimumFrameRate() << " to " << i.maximumFrameRate() << ")";
            if(std::find(desiredFormats.begin(), desiredFormats.end(), i.pixelFormat()) != desiredFormats.end())
            {
                int area = (i.resolution().height() * i.resolution().width());
                if(area > best.first)
                {
                    best = { area, i };
                }
            }
        }
        assert(!best.second.isNull());

        camera->setViewfinderSettings(best.second);
    }

    auto frameHandlers = FrameHandlerManager::get();
    if(frameHandlers)
    {
        QCameraInfo cameraInfo( *camera );
        frameHandlers->setOrientation(cameraInfo.orientation());
    }

    view.showFullScreen();
    
    return app.exec();
}

if(NOT GATHERER_OPENGL_ES)
  add_subdirectory(ogles_gpgpu_test)
endif()

if(BUILD_QT)

  # Disable until qtdeclarative is available: https://github.com/ruslo/hunter/issues/253
  #add_subdirectory(qt-camera-demo)

  # Try this stock camera sample copy-and-paste from: 
  # http://doc.qt.io/qt-5/qtmultimediawidgets-camera-example.html

  # Currently not linking, working through plugin issues and lack of Qt5MultimediaQuick_p
  if(GATHERER_OPENGL_ES)
    add_subdirectory(declarative-camera)
  else()
    # See issue #55
  endif()

  add_subdirectory(qmlvideofilter)
endif()

add_subdirectory(thread-pool)
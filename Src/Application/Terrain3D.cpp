//==================================================================================================================|
// Created 2014.04.26 by Daniel L. Watkins
//
// Copyright (C) 2014 Daniel L. Watkins
// This file is licensed under the MIT License.
//==================================================================================================================|

#include "Terrain3D.h"
#include <Core/Core.h>
#include <QuickItems/CameraItem.h>
#include <QQmlContext>

namespace t3d
{
//========================================
// Public
//========================================
	Terrain3D::Terrain3D(Settings *mainSettings) :
		mPreviouslyHadFocus(false),
		mNeedsRestart(false),
		mMainSettings(mainSettings),
		mCameraItem(nullptr)
	{
		mMainSettings->addListener(this);
	}


	Terrain3D::~Terrain3D()
	{
		backgroundUpdater.requestInterruption();
		backgroundUpdater.wait(1000);
	}


	void Terrain3D::init()
	{
		QSurfaceFormat format;
		format.setMajorVersion(4);
		format.setMinorVersion(2);
		format.setProfile(QSurfaceFormat::CompatibilityProfile);
		setFormat(format);

		setPersistentOpenGLContext(true);
		setPersistentSceneGraph(true);

		setResizeMode(QQuickView::SizeRootObjectToView);
		setSource(QUrl("qrc:///main.qml"));

		QWindow::show();
		theCamera = new World::Camera;

		loadUserSettings();

		mWorld.init(buildWorldConfiguration());
		theCamera->setWorld(&mWorld);
		theCamera->init(buildCameraConfiguration());

		connect(&backgroundUpdater, &BackgroundUpdater::needsUpdate,
				this, &Terrain3D::willUpdate);
		backgroundUpdater.start();
	}


	void Terrain3D::toggleCaptureCursor()
	{
		setCapturesCursor(!capturesCursor());
		//TODO add as setting
	}


	void Terrain3D::toggleFullscreen()
	{
		if (QWindow::visibility() == QWindow::FullScreen)
			QWindow::showNormal();
		else
			QWindow::showFullScreen();

		mMainSettings->setValue(Settings::Key::GraphicsScreenIsFullscreen,
								QWindow::visibility() == QWindow::FullScreen);

		emit refreshSettingsMenu();
	}


	void Terrain3D::toggleWireframe()
	{
		using namespace World::Terrain;
		World::Camera *camera = theCamera;

		camera->mode() == Mode::Normal ?
					camera->setMode(Mode::WireFrame) :
					camera->setMode(Mode::Normal);

		mMainSettings->setValue(Settings::Key::GraphicsCameraWireframe,
								camera->mode() == Mode::WireFrame);

		emit refreshSettingsMenu();
	}


	void Terrain3D::settingsValueChanged(Settings::Key key, const QVariant &value)
	{
		#define CASE(k) case Settings::k:

		switch (key)
		{
			//graphics
			CASE(GraphicsScreenResolutionWidth) {
				QWindow::resize(value.toInt(), QWindow::height());
				break;
			}

			CASE(GraphicsScreenResolutionHeight) {
				QWindow::resize(QWindow::width(), value.toInt());
				break;
			}

			CASE(GraphicsScreenIsFullscreen) {
				value.toBool() ? QWindow::showFullScreen() : QWindow::showNormal();
				break;
			}

			CASE(GraphicsCameraPositionX) {
				Vec3f c = theCamera->position();
				theCamera->setPosition(Vec3f(value.toFloat(), c.y, c.z));
				break;
			}

			CASE(GraphicsCameraPositionY) {
				Vec3f c = theCamera->position();
				theCamera->setPosition(Vec3f(c.x, value.toFloat(), c.z));
				break;
			}

			CASE(GraphicsCameraPositionZ) {
				Vec3f c = theCamera->position();
				theCamera->setPosition(Vec3f(c.x, c.y, value.toFloat()));
				break;
			}

			CASE(GraphicsCameraFOV) {
				theCamera->setFieldOfView(value.toFloat());
				break;
			}

			CASE(GraphicsCameraLOD) {
				//TODO implement a LOD factor!
				break;
			}
			CASE(GraphicsCameraWireframe) {
				theCamera->setMode(value.toBool()
											   ? World::Terrain::Mode::WireFrame :
												 World::Terrain::Mode::Normal);
			}


			//world
			CASE(WorldGeneratorSize) {
				//TODO
				break;
			}

			CASE(WorldGeneratorTextureMapResolution) {
				//TODO
				break;
			}

			CASE(WorldGeneratorSeed) {
				//TODO
				break;
			}

			CASE(WorldTerrainSpacing) {
				//TODO
				break;
			}

			CASE(WorldTerrainHeightScale) {
				//TODO
				break;
			}

			CASE(WorldTerrainBlockSize) {
				//TODO
				break;
			}

			CASE(WorldTerrainSpanSize) {
				//TODO
				break;
			}
		}

		#undef CASE
	}


//========================================
// Private
//========================================
	void Terrain3D::keyPressEvent(QKeyEvent *ev)
	{
		QQuickView::keyPressEvent(ev);

		using namespace World::Terrain;
		const float speed = 1.75f;

		//update the camera
		auto cameraPtr = theCamera;
		if (cameraPtr && !mCameraItem->isFrozen())
		{
			switch (ev->key())
			{
				case Qt::Key_W:
					cameraPtr->incPosition(speed * cameraPtr->forward()); break;
				case Qt::Key_S:
					cameraPtr->incPosition(speed * -cameraPtr->forward()); break;
				case Qt::Key_A:
					cameraPtr->incPosition(speed * -cameraPtr->right()); break;
				case Qt::Key_D:
					cameraPtr->incPosition(speed * cameraPtr->right()); break;

				case Qt::Key_X:
					toggleWireframe();
			}
		}

		switch (ev->key())
		{
			//quit
			case Qt::Key_Escape:
				close(); break;

			//toggle cursor capture
			case Qt::Key_F1:
				toggleCaptureCursor(); break;

			//restart
			case Qt::Key_F5:
				requestRestart(); break;

			//toggle settings menu
			case Qt::Key_F10:
				emit toggleSettingsMenu(); break;

			//toggle fullscreen
			case Qt::Key_F11: toggleFullscreen(); break;
		}
	}


	void Terrain3D::updateCursorPos()
	{
		if (mCameraItem->isFrozen() || !capturesCursor() && mouseButtonLeftPressed() == false)
			return;

		if (QWindow::isActive())
		{
			if (!mPreviouslyHadFocus)
			{
				resetCursorPosition();
			}
			else
			{
				if (theCamera)
				{
					const double mouseSensitivity = 0.1f;
					QVector2D delta = consumeCursorDelta();
					theCamera->incOrientation(delta.x()*mouseSensitivity, delta.y()*mouseSensitivity);

					resetCursorPosition();
				}
			}

			mPreviouslyHadFocus = true;
		}
		else
		{
			mPreviouslyHadFocus = false;
		}
	}


	void Terrain3D::loadUserSettings()
	{
		const QMetaObject &mo = Settings::staticMetaObject;
		QMetaEnum me = mo.enumerator(mo.indexOfEnumerator("Key"));

		//loop through every Settings::Key and tell ourself that a value has
		//changed to effectively load the value
		for (int i=0; i<me.keyCount(); i++)
		{
			Settings::Key key = static_cast<Settings::Key>(me.value(i));
			settingsValueChanged(key, mMainSettings->value(key));
		}
	}


	World::Camera::Configuration Terrain3D::buildCameraConfiguration()
	{
		typedef Settings::Key key;
		World::Camera::Configuration config;

		config.terrainBlockSize = mMainSettings->value(key::WorldTerrainBlockSize).toInt();
		config.terrainHeightScale = mMainSettings->value(key::WorldTerrainHeightScale).toFloat();
		config.terrainSpacing = mMainSettings->value(key::WorldTerrainSpacing).toFloat();
		config.terrainSpanSize = mMainSettings->value(key::WorldTerrainSpanSize).toInt();

		return config;
	}


	World::World::Configuration Terrain3D::buildWorldConfiguration()
	{
		typedef Settings::Key key;
		World::World::Configuration config;

		config.generatorSize = mMainSettings->value(key::WorldGeneratorSize).toInt();
		config.generatorTextureMapResolution = mMainSettings->value(key::WorldGeneratorTextureMapResolution).toInt();
		config.generatorSmoothing = mMainSettings->value(key::WorldGeneratorSmoothing).toFloat();
		config.generatorFaultCount = mMainSettings->value(key::WorldGeneratorFaultCount).toInt();
		config.generatorSeed = mMainSettings->value(key::WorldGeneratorSeed).toInt();
		config.generatorLightIntensity = mMainSettings->value(key::WorldTerrainLightIntensity).toFloat();

		return config;
	}


//========================================
// Public Slots
//========================================
	void Terrain3D::willUpdate()
	{
		if (mCameraItem == nullptr)
		{
			mCameraItem = rootObject()->findChild<QuickItems::CameraItem*>("t3d_mainCamera");
		}
		else
		{
			updateCursorPos();
		}
	}
}

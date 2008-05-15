/*
 * Stellarium
 * Copyright (C) 2007 Fabien Chereau
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <config.h>

#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelFileMgr.hpp"
#include "Projector.hpp"
#include "fixx11h.h"
#include "StelAppGraphicsItem.hpp"
#include "ViewportDistorter.hpp"
#include "StelModuleMgr.hpp"
#include "StelMainGraphicsView.hpp"
// #include "ui_viewDialog.h"

#include <QtOpenGL>
#include <QTimer>

StelAppGraphicsItem* StelAppGraphicsItem::singleton = NULL;
 
StelAppGraphicsItem::StelAppGraphicsItem() 
{
	assert(!singleton);
	singleton = this;
	
	distorter = ViewportDistorter::create("none",800,600,NULL);
	lastEventTimeSec = StelApp::getInstance().getTotalRunTime();
	previousTime = lastEventTimeSec;
	mainTimer = new QTimer(this);
	connect(mainTimer, SIGNAL(timeout()), this, SLOT(recompute()));
	
	setAcceptsHoverEvents(true);
	setFlags(QGraphicsItem::ItemIsFocusable);
	setZValue(-1);
}

StelAppGraphicsItem::~StelAppGraphicsItem()
{
	if (distorter)
	{
		delete distorter;
		distorter=NULL;
	}
}

void StelAppGraphicsItem::init()
{
	setViewPortDistorterType(StelApp::getInstance().getSettings()->value("video/distorter","none").toString());
}

void StelAppGraphicsItem::glWindowHasBeenResized(int w, int h)
{
	setRect(QRect(0,0,w,h));
	if (!distorter || (distorter && distorter->getType() == "none"))
	{
		StelApp::getInstance().glWindowHasBeenResized(w, h);
	}
}

//! Paint the whole Core of stellarium
//! This method is called automatically by the GraphicsView
void StelAppGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	scene()->views().at(0)->setViewportUpdateMode(QGraphicsView::NoViewportUpdate);
	
	const double now = StelApp::getInstance().getTotalRunTime();
	double dt = now-previousTime;
	previousTime = now;
	if (dt<0)	// This fix the star scale bug!!
		return;
	
	// Update the core and all modules
	StelApp::getInstance().update(dt);
	
	// Save openGL projection state
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glShadeModel(GL_SMOOTH);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_LINE_SMOOTH);
	glDisable(GL_LIGHTING);
	glDisable(GL_MULTISAMPLE);
	glDisable(GL_DITHER);
	glDisable(GL_ALPHA_TEST);
	
	StelApp::getInstance().glWindowHasBeenResized((int)(rect().width()), (int)(rect().height()));
	
// 	glClearColor(0.0, 0.0, 0.0, 0.0);
// 	glClear(GL_COLOR_BUFFER_BIT);
// 	glWidget->swapBuffers();
// 	glClear(GL_COLOR_BUFFER_BIT);
	
	// And draw them
	distorter->prepare();
	StelApp::getInstance().draw();
	distorter->distort();
	
	// Restore openGL projection state for Qt drawings
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glPopAttrib();
}

void StelAppGraphicsItem::recompute()
{
	// Update the whole scene because the openGL background is fully rendered
	scene()->views().at(0)->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
	update(rect());
	
	
	double duration = 1./StelApp::getInstance().minfps;
	if (StelApp::getInstance().getTotalRunTime()-lastEventTimeSec<2.5)
		duration = 1./StelApp::getInstance().maxfps;
	int dur = (int)(duration*1000);
	mainTimer->start(dur<5 ? 5 : dur);
}

void StelAppGraphicsItem::thereWasAnEvent()
{
	// Refresh screen ASAP
	recompute();
	lastEventTimeSec = StelApp::getInstance().getTotalRunTime();
}

void StelAppGraphicsItem::setViewPortDistorterType(const QString &type)
{
	if (type != getViewPortDistorterType())
	{
		if (type == "none")
		{
			StelMainGraphicsView::getInstance().getOpenGLWin()->setMaximumSize(10000,10000);
		}
		else
		{
			StelMainGraphicsView::getInstance().getOpenGLWin()->setFixedSize((int)(rect().width()), (int)(rect().height()));
		}
	}
	if (distorter)
	{
		delete distorter;
		distorter = NULL;
	}
	distorter = ViewportDistorter::create(type,(int)(rect().width()),(int)(rect().height()),StelApp::getInstance().getCore()->getProjection());
}

QString StelAppGraphicsItem::getViewPortDistorterType() const
{
	if (distorter)
		return distorter->getType();
	return "none";
}

// Set mouse cursor display
void StelAppGraphicsItem::showCursor(bool b)
{
	setCursor(b ? Qt::ArrowCursor : Qt::BlankCursor);
}

// Start the main drawing loop
void StelAppGraphicsItem::startDrawingLoop()
{
	mainTimer->start(5);
}

bool StelAppGraphicsItem::sceneEvent(QEvent* event)
{
	if (event->type()==QEvent::GraphicsSceneMouseMove)
	{
		QGraphicsSceneMouseEvent* mevent = (QGraphicsSceneMouseEvent*)event;
		int x = (int)mevent->pos().x();
		int y = (int)mevent->pos().y();
		y = (int)rect().height() - 1 - y;
		distorter->distortXY(x,y);
		StelApp::getInstance().handleMove(x, y);
	}
	if (event->type()==QEvent::GraphicsSceneHoverMove)
	{
		QGraphicsSceneHoverEvent* mevent = (QGraphicsSceneHoverEvent*)event;
		int x = (int)mevent->pos().x();
		int y = (int)mevent->pos().y();
		y = (int)rect().height() - 1 - y;
		distorter->distortXY(x,y);
		StelApp::getInstance().handleMove(x, y);
	}
	return QGraphicsItem::sceneEvent(event);
}

void StelAppGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	int x = (int)event->pos().x();
	int y = (int)event->pos().y();
	
	y = height() - 1 - y;
	distorter->distortXY(x,y);
	QMouseEvent newEvent(QEvent::MouseButtonPress, QPoint(x,y), event->button(), event->buttons(), event->modifiers());
	StelApp::getInstance().handleClick(&newEvent);

	event->setAccepted(true);
	
	// Refresh screen ASAP
	thereWasAnEvent();
}

void StelAppGraphicsItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	int x = (int)event->pos().x();
	int y = (int)event->pos().y();
	
	y = height() - 1 - y;
	distorter->distortXY(x,y);
	QMouseEvent newEvent(QEvent::MouseButtonRelease, QPoint(x,y), event->button(), event->buttons(), event->modifiers());
	StelApp::getInstance().handleClick(&newEvent);
	
	// Refresh screen ASAP
	thereWasAnEvent();
}

void StelAppGraphicsItem::wheelEvent(QGraphicsSceneWheelEvent* event)
{
	int x = (int)event->pos().x();
	int y = (int)event->pos().y();
	
	y = (int)rect().height() - 1 - y;
	distorter->distortXY(x,y);
	QWheelEvent newEvent(QPoint(x,y), event->delta(), event->buttons(), event->modifiers(), event->orientation());
	StelApp::getInstance().handleWheel(&newEvent);
	
	// Refresh screen ASAP
	thereWasAnEvent();
}

void StelAppGraphicsItem::keyPressEvent(QKeyEvent* event)
{
	StelApp::getInstance().handleKeys(event);
	// Refresh screen ASAP
	thereWasAnEvent();
}

void StelAppGraphicsItem::keyReleaseEvent(QKeyEvent* event)
{
	StelApp::getInstance().handleKeys(event);
	// Refresh screen ASAP
	thereWasAnEvent();
}

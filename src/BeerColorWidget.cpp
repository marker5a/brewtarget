/*
 * BeerColorWidget.cpp is part of Brewtarget, and is Copyright Philip G. Lee
 * (rocketman768@gmail.com), 2009.
 *
 * Brewtarget is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Brewtarget is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QWidget>
#include <QPixmap>
#include <QPaintEvent>
#include <QPainter>
#include <QRect>
#include <QPoint>
#include <QColor>
#include <QSize>
#include <QSizePolicy>
#include "BeerColorWidget.h"
#include "config.h"

// TODO: make the size adjust inside the container.
BeerColorWidget::BeerColorWidget()
{
   //setFixedSize(QSize(90,130));
   setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
   setMinimumSize(90, 130);
   
   glass = QImage(GLASS);
   recObs = 0;
}

void BeerColorWidget::setRecipe( Recipe* rec )
{
   setObserved(rec);
   recObs = rec;
   if( recObs )
      setColor( recObs->getSRMColor() );
}

void BeerColorWidget::notify(Observable* notifier, QVariant info)
{
   if( notifier != recObs )
      return;
   
   setColor( recObs->getSRMColor() );
}

void BeerColorWidget::paintEvent(QPaintEvent *)
{
   QPainter painter(this);
   QRect rect;

   int x1 = (size().width() - 90) / 2;
   int y1 = 0;
   rect.setCoords( x1, y1, x1+87, y1+130 );
   painter.setBrush(color);
   painter.drawRect(rect);

   painter.drawImage( QPoint(x1,y1), glass );
}

void BeerColorWidget::setColor( QColor newColor )
{
   color = QColor(newColor);
   
   repaint();
}
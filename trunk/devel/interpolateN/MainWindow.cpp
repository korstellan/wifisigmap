#include <QtGui>

#define qDrawTextCp(p,pnt,string,c1,c2) 	\
	p.setPen(c1);			\
	p.drawText(pnt + QPoint(-1, -1), string);	\
	p.drawText(pnt + QPoint(+1, -1), string);	\
	p.drawText(pnt + QPoint(+1, +1), string);	\
	p.drawText(pnt + QPoint(-1, +1), string);	\
	p.setPen(c2);			\
	p.drawText(pnt, string);	\
	
#define qDrawTextOp(p,pnt,string) 	\
	qDrawTextCp(p,pnt,string,Qt::black,Qt::white);

class MainWindow : public QLabel {
	public:
		MainWindow();
};

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	MainWindow mw;
	mw.show();
	mw.adjustSize();
	return app.exec();
}

#define fuzzyIsEqual(a,b) ( fabs(a - b) < 0.00001 )
//#define fuzzyIsEqual(a,b) ( ((int) a) == ((int) b) )

class qPointValue {
public:
	qPointValue(QPointF p = QPointF()) : point(p), value(0.0), valueIsNull(true) {}
	qPointValue(QPointF p, double v)   : point(p), value(v),   valueIsNull(false) {}
	
	QPointF point;
	double  value;
	bool    valueIsNull;
	
	void setValue(double v)
	{
		value       = v;
		valueIsNull = false;
	}
	
	bool isNull()  { return  point.isNull() ||  valueIsNull; }
	bool isValid() { return !point.isNull() && !valueIsNull; }
};

class qQuadValue : public QRectF {
public:
	qQuadValue(
		qPointValue _tl,
		qPointValue _tr,
		qPointValue _br,
		qPointValue _bl)
		
		: QRectF(_tl.point.x(),  _tl.point.y(),
		         _tr.point.x() - _tl.point.x(),
		         _br.point.y() - _tr.point.y())
		
		, tl(_tl)
		, tr(_tr)
		, br(_br)
		, bl(_bl)
	{}
	
	qQuadValue(QList<qPointValue> corners)
	
		: QRectF(corners[0].point.x(),  corners[0].point.y(),
		         corners[1].point.x() - corners[0].point.x(),
		         corners[2].point.y() - corners[1].point.y())
		
		, tl(corners[0])
		, tr(corners[1])
		, br(corners[2])
		, bl(corners[3])
	{}
		
	QList<qPointValue> corners()
	{
		return QList<qPointValue>()
			<< tl << tr << br << bl;
	};
	
	qPointValue tl, tr, br, bl;
};

namespace SurfaceInterpolate {

/// \brief Simple value->color function
QColor colorForValue(double v)
{
	int hue = qMax(0, qMin(359, (int)((1-v) * 359)));
	//return QColor::fromHsv(hue, 255, 255);
	
	//int hue = (int)((1-v) * (359-120) + 120);
 	//hue = qMax(0, qMin(359, hue));

	//int hue = (int)(v * 120);
	//hue = qMax(0, qMin(120, hue));
 	 
	int sat = 255;
	int val = 255;
	double valTop = 0.9;
	if(v < valTop)
	{
		val = (int)(v/valTop * 255.);
		val = qMax(0, qMin(255, val));
	}
	
	return QColor::fromHsv(hue, sat, val);
}


// The following four cubic interpolation functions are from http://www.paulinternet.nl/?page=bicubic
double cubicInterpolate (double p[4], double x) {
	return p[1] + 0.5 * x*(p[2] - p[0] + x*(2.0*p[0] - 5.0*p[1] + 4.0*p[2] - p[3] + x*(3.0*(p[1] - p[2]) + p[3] - p[0])));
}

double bicubicInterpolate (double p[4][4], double x, double y) {
	double arr[4];
	arr[0] = cubicInterpolate(p[0], y);
	arr[1] = cubicInterpolate(p[1], y);
	arr[2] = cubicInterpolate(p[2], y);
	arr[3] = cubicInterpolate(p[3], y);
	return cubicInterpolate(arr, x);
}

/*
/// Not used:
double tricubicInterpolate(double p[4][4][4], double x, double y, double z) {
	double arr[4];
	arr[0] = bicubicInterpolate(p[0], y, z);
	arr[1] = bicubicInterpolate(p[1], y, z);
	arr[2] = bicubicInterpolate(p[2], y, z);
	arr[3] = bicubicInterpolate(p[3], y, z);
	return cubicInterpolate(arr, x);
}

/// Not used:
double nCubicInterpolate (int n, double* p, double coordinates[]) {
	//assert(n > 0);
	if(n <= 0)
		return -1;
	if (n == 1) {
		return cubicInterpolate(p, *coordinates);
	}
	else {
		double arr[4];
		int skip = 1 << (n - 1) * 2;
		arr[0] = nCubicInterpolate(n - 1, p, coordinates + 1);
		arr[1] = nCubicInterpolate(n - 1, p + skip, coordinates + 1);
		arr[2] = nCubicInterpolate(n - 1, p + 2*skip, coordinates + 1);
		arr[3] = nCubicInterpolate(n - 1, p + 3*skip, coordinates + 1);
		return cubicInterpolate(arr, *coordinates);
	}
}
*/

//bool newQuad = false;
/// \brief Interpolate the value for point \a x, \a y from the values at the four corners of \a quad
double quadInterpolate(qQuadValue quad, double x, double y)
{
	double w = quad.width();
	double h = quad.height();
	
	bool useBicubic = true;
	if(useBicubic)
	{
		// Use bicubic impl from http://www.paulinternet.nl/?page=bicubic
		double unitX = (x - quad.left() ) / w;
		double unitY = (y - quad.top()  ) / h;
		
		double	c1 = quad.tl.value,
			c2 = quad.tr.value,
			c3 = quad.br.value,
			c4 = quad.bl.value;

		double p[4][4] = 
		{
// 			{c1,c1,c2,c2},
// 			{c1,c1,c2,c2},
// 			{c4,c4,c3,c3},
// 			{c4,c4,c3,c3}
			
			// Note: X runs DOWN, values below are (x1,y1)->(x1,y4), NOT (x1,y1)->(x4,y1) as above (commented out) 
			{c1,c1,c4,c4},
			{c1,c1,c4,c4},
			{c2,c2,c3,c3},
			{c2,c2,c3,c3}
		};
		
		double p2 = bicubicInterpolate(p, unitX, unitY);
		//if(p2 > 1.0 || unitX > 1. || unitY > 1. || newQuad)
		//if(newQuad && unitX > 0.99)
		//{
			//qDebug() << "bicubicInterpolate: unit:"<<unitX<<","<<unitY<<": p2:"<<p2<<", w:"<<w<<", h:"<<h<<", x:"<<x<<", y:"<<y<<", corners:"<<corners[0].point<<corners[1].point<<corners[2].point<<corners[3].point<<", v:"<<c1<<c2<<c3<<c4;
			
			//newQuad = false;
		//}
		
		return p2;
	}
	else
	{
		// Very simple implementation of http://en.wikipedia.org/wiki/Bilinear_interpolation
		double fr1 = (quad.br.point.x() - x) / w * quad.tl.value
			   + (x - quad.tl.point.x()) / w * quad.tr.value;
	
		double fr2 = (quad.br.point.x() - x) / w * quad.bl.value
			   + (x - quad.tl.point.x()) / w * quad.br.value;
	
		double p1  = (quad.br.point.y() - y) / h * fr1
			   + (y - quad.tl.point.y()) / h * fr2;
	
		//qDebug() << "quadInterpolate: "<<x<<" x "<<y<<": "<<p<<" (fr1:"<<fr1<<",fr2:"<<fr2<<",w:"<<w<<",h:"<<h<<")";
		return p1;
	}
}

/// \brief Internal comparitor for sorting qPointValues
bool _qPointValue_sort_point(qPointValue a, qPointValue b)
{
	return  fuzzyIsEqual(a.point.y(), (int)b.point.y()) ?
		(int)a.point.x()  < (int)b.point.x() :
		(int)a.point.y()  < (int)b.point.y();
}

/// \brief Checks \a list to see if it contains \a point, \returns true if \a point is in \a list, false otherwise 
bool hasPoint(QList<qPointValue> list, QPointF point)
{
	foreach(qPointValue v, list)
		if(v.point == point)
			return true;
	return false;
}

/// \brief Calculates the weightof point \a o at point \a i given power \a p, used internally by interpolateValue()
double weight(QPointF i, QPointF o, double p)
{
	return 1/pow(QLineF(i,o).length(), p);
}

/// \brief A very simple IDW implementation to interpolate the value at \a point from the inverse weighted distance of all the other points given in \a inputs, using a default power factor 
double interpolateValue(QPointF point, QList<qPointValue> inputs)
{
	/*
	http://en.wikipedia.org/wiki/Inverse_distance_weighting:
	u(x) = sum(i=0..N, (
		W(i,x) * u(i)
		----------
		sum(j=0..N, W(j,x))
		)
	
	Where:
	W(i,X) = 1/(d(X,Xi)^p
	*/
	double p = 3;
	int n = inputs.size();
	double sum = 0;
	for(int i=0; i<n; i++)
	{
		double a = weight(inputs[i].point, point, p) * inputs[i].value;
		double b = 0;
		for(int j=0; j<n; j++)
			b += weight(inputs[j].point, point, p);
		sum += a/b;
	}
	return sum;
}

QList<qPointValue> testLine(QList<qPointValue> inputs, QList<qPointValue> existingPoints, QRectF bounds, QPointF p1, QPointF p2, int dir)
{
	QLineF boundLine(p1,p2);
	QList<qPointValue> outputs;
	
	if(!hasPoint(existingPoints, boundLine.p1()) &&
	   !hasPoint(outputs,        boundLine.p1()))
	{
		double value = interpolateValue(boundLine.p1(), inputs);
		outputs << qPointValue(boundLine.p1(), value);
	}
	
	if(!hasPoint(existingPoints, boundLine.p2()) &&
	   !hasPoint(outputs,        boundLine.p2()))
	{
		double value = interpolateValue(boundLine.p2(), inputs);
		outputs << qPointValue(boundLine.p2(), value);;
	}
	
	foreach(qPointValue v, inputs)
	{
		QLineF line;
		if(dir == 1)
			// line from point to TOP
			// (boundLine runs right-left on TOP side of bounds)
			line = QLineF(v.point, QPointF(v.point.x(), bounds.top()));
		else
		if(dir == 2)
			// line from point to RIGHT
			// (boundLine runs top-bottom on RIGHT side of bounds)
			line = QLineF(v.point, QPointF(bounds.right(), v.point.y()));
		else
		if(dir == 3)
			// line from point to BOTTOM
			// (boundLine runs right-left on BOTTOM side of bounds)
			line = QLineF(v.point, QPointF(v.point.x(), bounds.bottom()));
		else
		if(dir == 4)
			// line from point to LEFT
			// (boundLine runs top-bottom on LEFT side of bounds)
			line = QLineF(v.point, QPointF(bounds.left(),  v.point.y()));
		
		// test to see boundLine intersects line, if so, create point if none exists
		QPointF point;
		/*QLineF::IntersectType type = */boundLine.intersect(line, &point);
		if(!point.isNull())
		{
			if(!hasPoint(existingPoints, point) &&
			   !hasPoint(outputs, point))
			{
// 				double unitVal = (dir == 1 || dir == 3 ? v.point.x() : v.point.y()) / lineLength;
// 				double value = cubicInterpolate(pval, unitVal);
				double value = interpolateValue(point, inputs);
				outputs << qPointValue(point, value);
				//qDebug() << "testLine: "<<boundLine<<" -> "<<line<<" ["<<dir<<"/1] New point: "<<point<<", value:"<<value<<", corners: "<<c1.value<<","<<c2.value;
			}
		}
		
		foreach(qPointValue v2, inputs)
		{
			QLineF line2;
			if(dir == 1 || dir == 3)
				// 1=(boundLine runs right-left on TOP side of bounds)
				// 3=(boundLine runs right-left on BOTTOM side of bounds)
				// run line left right, see if intersects with 'line' above
				line2 = QLineF(bounds.left(), v2.point.y(), bounds.right(), v2.point.y());
			else
			if(dir == 2 || dir == 4)
				// 2=(boundLine runs top-bottom on RIGHT side of bounds)
				// 4=(boundLine runs top-bottom on LEFT side of bounds)
				// run line left right, see if intersects with 'line' above
				line2 = QLineF(v2.point.x(), bounds.top(), v2.point.x(), bounds.bottom());
			
			//qDebug() << "testLine: "<<boundLine<<" -> "<<line<<" ["<<dir<<"/2] Testing: "<<line2;
			
			// test to see boundLine intersects line, if so, create point if none exists
			QPointF point2;
			/*QLineF::IntersectType type = */line.intersect(line2, &point2);
			if(!point2.isNull())
			{
				if(!hasPoint(existingPoints, point2) &&
				   !hasPoint(outputs, point2))
				{
					double value = interpolateValue(point2, inputs);
					outputs << qPointValue(point2, isnan(value) ? 0 :value);
				}
			}
		}
		
	}
	
	
	return outputs;
}

qPointValue nearestPoint(QList<qPointValue> list, QPointF point, int dir, bool requireValid=false)
{
	/// \a dir: 0=X+, 1=Y+, 2=X-, 3=Y-
	
	qPointValue minPnt;
	double minDist = (double)INT_MAX;
	double origin  = dir == 0 || dir == 2 ? point.x() : point.y();

	foreach(qPointValue v, list)
	{
		if(requireValid && !v.isValid())
			continue;
			
		double val  = dir == 0 || dir == 2 ? v.point.x() : v.point.y();
		double dist = fabs(val - origin);
		if(((dir == 0 && fuzzyIsEqual(v.point.y(), point.y()) && val > origin) ||
		    (dir == 1 && fuzzyIsEqual(v.point.x(), point.x()) && val > origin) ||
		    (dir == 2 && fuzzyIsEqual(v.point.y(), point.y()) && val < origin) ||
		    (dir == 3 && fuzzyIsEqual(v.point.x(), point.x()) && val < origin) )  &&
		    dist < minDist)
		{
			minPnt = v;
			minDist = dist;
		}
	}
	
	return minPnt;
}

/// \brief Returns the bounds (max/min X/Y) of \a points
QRectF getBounds(QList<qPointValue> points)
{
	QRectF bounds;
	foreach(qPointValue v, points)
	{
		//qDebug() << "corner: "<<v.point<<", value:"<<v.value;
		bounds |= QRectF(v.point, QSizeF(1.,1.0));
	}
	bounds.adjust(0,0,-1,-1);
	
	return bounds;
}
	


/**
\brief Generates a list of quads covering the every point given by \a points using rectangle bicubic interpolation.

	Rectangle Bicubic Interpolation
	
	The goal:
	Given an arbitrary list of points (and values assoicated with each point),
	interpolate the value for every point contained within the bounding rectangle.
	Goal something like: http://en.wikipedia.org/wiki/File:BicubicInterpolationExample.png
	
	To illustrate what this means and how we do it, assume you give it four points (marked by asterisks, below),
	along with an arbitrary value for each point:
	
	*-----------------------
	|                       |
	|                       |
	|       *               |
	|                       |
	|                       |
	|                       |
	|                       |
	|             *         |
	|                       |
	|                       |
	 -----------------------*
	
	The goal of the algorithm is to subdivide the bounding rectangle (described as the union of the location of all the points)
	into a series of sub-rectangles, and interpolate the values of the known points to the new points found by the intersections
	of the lines:
	
	X-------*-----*---------*
	|       |     |         |
	|       |     |         |
	*-------X-----*---------*
	|       |     |         |
	|       |     |         |
	|       |     |         |
	|       |     |         |
	*-------*-----X---------*
	|       |     |         |
	|       |     |         |
	*-------*-----*---------X
	
	Above, X marks the original points, and the asterisks marks the points the algorithm inserted to create the sub-rectangles.
	For each asterisk above, the algorithm also interpolated the values from each of the given points (X's) to find the value
	at the new point (using bicubic interpolation.)
	
	Now that the algorithm has a list of rectangles, it can procede to render each of them using bicubic interpolation of the value
	across the surface.
	
	Note that for any CORNER points which are NOT given, they are assumed to have a value of "0". This can be changed in "testLine()", above.
*/

QList<qQuadValue> generateQuads(QList<qPointValue> points)
{
	// Sorted Y1->Y2 then X1->X2 (so for every Y, points go X1->X2, then next Y, etc, for example: (0,0), (1,0), (0,1), (1,1))
	//qSort(points.begin(), points.end(), qPointValue_sort_point);
	
	// Find the bounds of the point cloud given so we can then subdivide it into smaller rectangles as needed:
	QRectF bounds = getBounds(points);
	
	// Iterate over every point
	// draw line from point to each bound (X1,Y),(X2,Y),(X,Y1),(X,Y2)
	// see if line intersects with perpendicular line from any other point
	// add point at intersection if none exists
	// add point at end point of line if none exists
	QList<qPointValue> outputList = points;
	
	outputList << testLine(points, outputList, bounds, bounds.topLeft(),		bounds.topRight(),     1);
	outputList << testLine(points, outputList, bounds, bounds.topRight(),		bounds.bottomRight(),  2);
	outputList << testLine(points, outputList, bounds, bounds.bottomRight(),	bounds.bottomLeft(),   3);
	outputList << testLine(points, outputList, bounds, bounds.bottomLeft(),		bounds.topLeft(),      4);
	
	/* After these four testLine() calls, the outputList now has these points (assuming the first example above):
	
	*-------X-----X---------X
	|       |     |         |
	|       |     |         |
	X-------*-----X---------X
	|       |     |         |
	|       |     |         |
	|       |     |         |
	|       |     |         |
	X-------X-----*---------X
	|       |     |         |
	|       |     |         |
	X-------X-----*---------*
	
	New points added by the four testLine() calls ar marked with an X.
	
	*/
	
	// Sort the list of ppoints so we procede thru the list top->bottom and left->right
	qSort(outputList.begin(), outputList.end(), _qPointValue_sort_point);
	
	// Points are now sorted Y1->Y2 then X1->X2 (so for every Y, points go X1->X2, then next Y, etc, for example: (0,0), (1,0), (0,1), (1,1))
	
	//qDebug() << "bounds:"<<bounds;

	//QList<QList<qPointValue> > quads;
	QList<qQuadValue> quads;
	
	// This is the last stage of the algorithm - go thru the new point cloud and construct the actual sub-rectangles
	// by starting with each point and proceding clockwise around the rectangle, getting the nearest point in each direction (X+,Y), (X,Y+) and (X-,Y)
	foreach(qPointValue tl, outputList)
	{
		QList<qPointValue> quad;
		
		qPointValue tr = nearestPoint(outputList, tl.point, 0);
		qPointValue br = nearestPoint(outputList, tr.point, 1);
		qPointValue bl = nearestPoint(outputList, br.point, 2);
		
		// These 'isNull()' tests catch points on the edges of the bounding rectangle
		if(!tr.point.isNull() &&
		   !br.point.isNull() &&
		   !bl.point.isNull())
		{
// 			quad  << tl << tr << br << bl;
// 			quads << (quad);
			quads << qQuadValue(tl, tr, br, bl);
			
			//qDebug() << "Quad[p]: "<<tl.point<<", "<<tr.point<<", "<<br.point<<", "<<bl.point;
// 			qDebug() << "Quad[v]: "<<tl.value<<tr.value<<br.value<<bl.value;
		}
	}
	
	return quads;
}

const QPointF operator*(const QPointF& a, const QPointF& b)
{
	return QPointF(a.x() * b.x(), a.y() * b.y());
}

const qPointValue operator*(const qPointValue& a, const QPointF& b)
{
	return qPointValue(a.point * b, a.value);
}

QImage renderPoints(QList<qPointValue> points, QSize renderSize = QSize())
{
	QRectF bounds = getBounds(points);
	
	QSizeF outputSize = bounds.size();
	double dx = 1, dy = 1;
	if(!renderSize.isNull())
	{
		outputSize.scale(renderSize, Qt::KeepAspectRatio);
		dx = outputSize.width()  / bounds.width();
		dy = outputSize.height() / bounds.height();
	}
	
	QPointF renderScale(dx,dy);
	
	QList<qQuadValue> quads = generateQuads(points);
	
	// Arbitrary rendering choices
//	QImage img(w,h, QImage::Format_ARGB32_Premultiplied);
	QImage img(outputSize.toSize(), QImage::Format_ARGB32_Premultiplied);
	
	QPainter p(&img);
	p.fillRect(img.rect(), Qt::white);
	
	int counter = 0, maxCounter = quads.size();
	
	//foreach(QList<qPointValue> quad, quads)
	foreach(qQuadValue quad, quads)
	{
 		qPointValue tl = quad.tl * renderScale; //quad[0];
 		qPointValue tr = quad.tr * renderScale; //quad[1];
		qPointValue br = quad.br * renderScale; //quad[2];
		qPointValue bl = quad.bl * renderScale; //quad[3];
		
		//qDebug() << "[quads]: pnt "<<(counter++)<<": "<<tl.point;
		int progress = (int)(((double)(counter++)/(double)maxCounter) * 100);
		qDebug() << "renderPoints(): Rendering rectangle, progress:"<<progress<<"%";
		
		int xmin = qMax((int)(tl.point.x()), 0);
		int xmax = qMin((int)(br.point.x()), (int)(img.width()));

		int ymin = qMax((int)(tl.point.y()), 0);
		int ymax = qMin((int)(br.point.y()), (int)(img.height()));


		// Here's the actual rendering of the interpolated quad
		for(int y=ymin; y<ymax; y++)
		{
			QRgb* scanline = (QRgb*)img.scanLine(y);
			for(int x=xmin; x<xmax; x++)
			{
				double value = quadInterpolate(quad, ((double)x) / dx, ((double)y) / dy);
				//QColor color = colorForValue(value);
				QColor color = colorForValue(value);
				scanline[x] = color.rgba();
			}
		}
			
		QVector<QPointF> vec;
		vec << tl.point << tr.point << br.point << bl.point;
		p.setPen(QColor(0,0,0,127));
		p.drawPolygon(vec);
// 
		p.setPen(Qt::gray);
		qDrawTextOp(p,tl.point, QString().sprintf("%.02f",tl.value));
		qDrawTextOp(p,tr.point, QString().sprintf("%.02f",tr.value));
		qDrawTextOp(p,bl.point, QString().sprintf("%.02f",bl.value));
		qDrawTextOp(p,br.point, QString().sprintf("%.02f",br.value));
	}

// 	p.setPen(QPen());
// 	p.setBrush(QColor(0,0,0,127));
// 	foreach(qPointValue v, points)
// 	{
// 		p.drawEllipse(v.point, 5, 5);
// 	}

// 	
// 	p.setPen(QPen());
// 	p.setBrush(QColor(255,255,255,200));
// 	counter = 0;
// 	foreach(qPointValue v, outputList)
// 	{
// 		p.setPen(QPen());
// 		if(!hasPoint(points, v.point))
// 			p.drawEllipse(v.point, 5, 5);
// 			
// // 		p.setPen(Qt::gray);
// // 		qDrawTextOp(p,v.point + QPointF(0, p.font().pointSizeF()*1.75), QString::number(counter++));
// 		
// 	}
	
	p.end();
	
	return img;
}

};

using namespace SurfaceInterpolate;

MainWindow::MainWindow()
{
	QRectF rect(0,0,4001,3420);
	//QRectF rect(0,0,300,300);
	//QRectF(89.856,539.641 1044.24x661.359)
	double w = rect.width(), h = rect.height();

	
	
	
	// Setup our list of points - these can be in any order, any arrangement.
	
	QList<qPointValue> points = QList<qPointValue>()
		<< qPointValue(QPointF(0,0), 		0.0)
// 		<< qPointValue(QPointF(10,80), 		0.5)
// 		<< qPointValue(QPointF(80,10), 		0.75)
// 		<< qPointValue(QPointF(122,254), 	0.33)
		<< qPointValue(QPointF(w,0), 		0.0)
		<< qPointValue(QPointF(w/3*1,h/3*2),	1.0)
		<< qPointValue(QPointF(w/2,h/2),	1.0)
		<< qPointValue(QPointF(w/3*2,h/3*1),	1.0)
		<< qPointValue(QPointF(w,h), 		0.0)
		<< qPointValue(QPointF(0,h), 		0.0);

// 	QList<qPointValue> points = QList<qPointValue>()
// // 		<< qPointValue(QPointF(0,0), 		0.0)
// // 
// // 		<< qPointValue(QPointF(w*.5,h*.25),	1.0)
// // 		<< qPointValue(QPointF(w*.5,h*.75),	1.0)
// // 
// // 		<< qPointValue(QPointF(w*.5,h*.5),	0.5)
// // 
// // 		<< qPointValue(QPointF(w*.25,h*.5),	1.0)
// // 		<< qPointValue(QPointF(w*.75,h*.5),	1.0)
// // 
// // 		<< qPointValue(QPointF(w,w),		0.0);
// 		<< qPointValue(QPointF(0,0), 		0.0)
// 
// // 		<< qPointValue(QPointF(w*.2,h*.2),	1.0)
// // 		<< qPointValue(QPointF(w*.5,h*.2),	1.0)
// // 		<< qPointValue(QPointF(w*.8,h*.2),	1.0)
// 
// 		<< qPointValue(QPointF(w*.2,h*.8),	1.0)
// 		<< qPointValue(QPointF(w*.5,h*.8),	1.0)
// 		<< qPointValue(QPointF(w*.8,h*.8),	1.0)
// 
// 		<< qPointValue(QPointF(w*.8,h*.5),	1.0)
// 		<< qPointValue(QPointF(w*.8,h*.2),	1.0)
// 		
// 		//<< qPointValue(QPointF(w*.75,h),	1.0)
// 
// 		<< qPointValue(QPointF(w,w),		0.0)
// 		;

  

//   	QList<qPointValue> points;// = QList<qPointValue>()
//   	points << qPointValue( QPointF(116.317, 2180.41), 0.6);
//         points << qPointValue( QPointF(408.87, 2939.64), 0.533333);
//         points << qPointValue( QPointF(71.2619, 3276.22), 0.52);
//         points << qPointValue( QPointF(326.109, 2682.47), 0.506667);
//         points << qPointValue( QPointF(246.38, 2017.56), 0.613333);
//         points << qPointValue( QPointF(210.603, 1790.57), 0.68);
//         points << qPointValue( QPointF(69.7606, 55.8084), 0.626667);
//         points << qPointValue( QPointF(1505.94, 59.3849), 0.68);
//         points << qPointValue( QPointF(2607.61, 53.2941), 0.693333);
//         points << qPointValue( QPointF(3934.88, 49.4874), 0.666667);
//         points << qPointValue( QPointF(3929.81, 1338.7), 0.693333);
//         points << qPointValue( QPointF(3942.07, 1899.13), 0.666667);
//         points << qPointValue( QPointF(2005.55, 1972.79), 0.906667);
//         points << qPointValue( QPointF(3811.29, 3186.99), 0.6);
// // 		<< qPointValue(QPointF(0,0), 		0.0)
// // 		<< qPointValue(QPointF(w,h),		0.0);
// 
// 	QList<qPointValue> originalInputs = points;
// 
// 	// Set bounds to be at minimum the bounds of the background
// 	points << qPointValue( QPointF(0,0), interpolateValue(QPointF(0,0), originalInputs ) );
// 	QPointF br = QPointF((double)w, (double)h);
// 	points << qPointValue( br, interpolateValue(br, originalInputs) );
// 	
// 	srand(time(NULL));

// 	for(int i=0; i<100; i++)
// 	{
// 		points << qPointValue(QPointF((double)rand()/(double)RAND_MAX*w, (double)rand()/(double)RAND_MAX*h), (double)rand()/(double)RAND_MAX);
// 		qDebug() << "points << qPointValue("<<points.last().point<<", "<<points.last().value<<");";
// 	}
	
// 	points << qPointValue( QPointF(77.8459, 259.802), 0.980021);
// 	points << qPointValue( QPointF(220.788, 23.7931), 0.445183);

// 	points << qPointValue( QPointF(136.454, 214.625), 0.684312);
// 	points << qPointValue( QPointF(2.11327, 169.876), 0.669569);
// 	points << qPointValue( QPointF(150.927, 244.475), 0.25839);
// 	points << qPointValue( QPointF(289.744, 107.131), 0.764456);
// 	points << qPointValue( QPointF(8.46319, 294.867), 0.996068);
//  



// points << qPointValue( QPointF(145.152, 881.28), 0.8);
// points << qPointValue( QPointF(89.856, 603.072), 0.183333);
// points << qPointValue( QPointF(361.152, 1007.42), 0.216667);
// points << qPointValue( QPointF(1134.1, 936.776), 0.1);
// points << qPointValue( QPointF(884.8, 789.019), 0.0166667);
// points << qPointValue( QPointF(832.851, 660.204), 0.183333);
// points << qPointValue( QPointF(944.251, 539.641), 0.05);
// points << qPointValue( QPointF(793.522, 979.344), 0.0833333);
// points << qPointValue( QPointF(746.287, 1065.78), 0.266667);
// points << qPointValue( QPointF(777.358, 1191.72), 0.1);
// points << qPointValue( QPointF(1057, 1201), 0.0166667);
// points << qPointValue( QPointF(975, 1043), 0.1);
// 
// 





	// Fuzzing, above, produced these points, which aren't rendred properly:
// 	points << qPointValue(QPointF(234.93, 118.315),  0.840188);
// 	qDebug() << "Added point: "<<points.last().point<<", val:"<<points.last().value;
// 	points << qPointValue(QPointF(59.2654, 273.494), 0.79844);
// 	qDebug() << "Added point: "<<points.last().point<<", val:"<<points.last().value;
	

	//exit(-1);


//  	#define val(x) ( ((double)x) / 6. )
//  	QList<qPointValue> points = QList<qPointValue>()
//  		<< qPointValue(QPointF(0,0),	val(0))
//  		
// 		<< qPointValue(QPointF(w/4*1,h/4*1),	val(1))
// 		<< qPointValue(QPointF(w/4*2,h/4*1),	val(2))
// 		<< qPointValue(QPointF(w/4*3,h/4*1),	val(4))
// 		<< qPointValue(QPointF(w/4*4,h/4*1),	val(1))
// 		
// 		<< qPointValue(QPointF(w/4*1,h/4*2),	val(6))
// 		<< qPointValue(QPointF(w/4*2,h/4*2),	val(3))
// 		<< qPointValue(QPointF(w/4*3,h/4*2),	val(5))
// 		<< qPointValue(QPointF(w/4*4,h/4*2),	val(2))
// 		
// 		<< qPointValue(QPointF(w/4*1,h/4*3),	val(4))
// 		<< qPointValue(QPointF(w/4*2,h/4*3),	val(2))
// 		<< qPointValue(QPointF(w/4*3,h/4*3),	val(1))
// 		<< qPointValue(QPointF(w/4*4,h/4*3),	val(5))
// 		
// 		<< qPointValue(QPointF(w/4*1,h/4*4),	val(5))
// 		<< qPointValue(QPointF(w/4*2,h/4*4),	val(4))
// 		<< qPointValue(QPointF(w/4*3,h/4*4),	val(2))
// 		<< qPointValue(QPointF(w/4*4,h/4*4),	val(3))
// 		
// 		;
// 
// 	#undef val
	
	
		
	QImage img = renderPoints(points, QSize(1000,1000));
	
	//exit(-1);
	/*
	
	// Just some debugging:
	
	QImage img(4,4, QImage::Format_ARGB32_Premultiplied);
	
	QList<QList<qPointValue> > quads;
	
	QList<qPointValue> points;
	
// 	points = QList<qPointValue>()
// 		<< qPointValue(QPointF(0,0), 					0.0)
// 		<< qPointValue(QPointF(img.width()/2,0),			0.5)
// 		<< qPointValue(QPointF(img.width()/2,img.height()), 		0.0)
// 		<< qPointValue(QPointF(0,img.height()), 			0.5);
// 	quads << points;
// 	
// 	points = QList<qPointValue>()
// 		<< qPointValue(QPointF(img.width()/2,0), 		0.5)
// 		<< qPointValue(QPointF(img.width(),0),			0.0)
// 		<< qPointValue(QPointF(img.width(),img.height()), 	0.5)
// 		<< qPointValue(QPointF(img.width()/2,img.height()), 	0.0);
// 	quads << points;


	points = QList<qPointValue>()
		<< qPointValue(QPointF(0,0), 				0.0)
		<< qPointValue(QPointF(img.width()-1,0),		0.0)
		<< qPointValue(QPointF(img.width()-1,img.height()-1), 	1.0)
		<< qPointValue(QPointF(0,img.height()-1), 		0.0);
	quads << points;
	
	
	//quads << (points);
	foreach(QList<qPointValue> corners, quads)
	{
		//for(int y=0; y<img.height(); y++)
		for(int y=corners[0].point.y(); y<=corners[2].point.y(); y++)
		{
			QRgb* scanline = (QRgb*)img.scanLine(y);
			for(int x=corners[0].point.x(); x<=corners[2].point.x(); x++)
			{
				double value = quadInterpolate(corners, (double)x, (double)y);
				qDebug() << "pnt:"<<x<<","<<y<<", val:"<<value;
				QColor color = colorForValue(value);
				scanline[x] = color.rgba();
			}
		}
	}
	*/
	img.save("interpolate.jpg");

	setPixmap(QPixmap::fromImage(img));//.scaled(200,200)));
}



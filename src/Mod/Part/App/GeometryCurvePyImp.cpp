/***************************************************************************
 *   Copyright (c) 2009 Werner Mayer <wmayer[at]users.sourceforge.net>     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"
#ifndef _PreComp_
# include <sstream>
# include <BRepBuilderAPI_MakeEdge.hxx>
# include <gp_Dir.hxx>
# include <gp_Vec.hxx>
# include <gp_Pln.hxx>
# include <GCPnts_UniformAbscissa.hxx>
# include <GCPnts_UniformDeflection.hxx>
# include <GCPnts_TangentialDeflection.hxx>
# include <GCPnts_QuasiUniformAbscissa.hxx>
# include <GCPnts_QuasiUniformDeflection.hxx>
# include <GCPnts_AbscissaPoint.hxx>
# include <Geom2dAPI_InterCurveCurve.hxx>
# include <GeomAPI.hxx>
# include <Geom_Geometry.hxx>
# include <Geom_Curve.hxx>
# include <Geom_Plane.hxx>
# include <Geom_Surface.hxx>
# include <GeomAdaptor_Curve.hxx>
# include <GeomFill.hxx>
# include <GeomLProp_CLProps.hxx>
# include <Handle_Geom_RectangularTrimmedSurface.hxx>
# include <Handle_Geom_BSplineSurface.hxx>
# include <Precision.hxx>
# include <GeomAPI_ProjectPointOnCurve.hxx>
# include <GeomConvert_ApproxCurve.hxx>
# include <Standard_Failure.hxx>
# include <Standard_NullValue.hxx>
# include <ShapeConstruct_Curve.hxx>
#endif

#include <Base/GeometryPyCXX.h>
#include <Base/VectorPy.h>

#include "Geometry.h"
#include "GeometryCurvePy.h"
#include "GeometryCurvePy.cpp"
#include "RectangularTrimmedSurfacePy.h"
#include "BSplineSurfacePy.h"
#include "PlanePy.h"
#include "BSplineCurvePy.h"

#include "TopoShape.h"
#include "TopoShapePy.h"
#include "TopoShapeEdgePy.h"

using namespace Part;

// returns a string which represents the object e.g. when printed in python
std::string GeometryCurvePy::representation(void) const
{
    return "<Curve object>";
}

PyObject *GeometryCurvePy::PyMake(struct _typeobject *, PyObject *, PyObject *)  // Python wrapper
{
    // never create such objects with the constructor
    PyErr_SetString(PyExc_RuntimeError,
        "You cannot create an instance of the abstract class 'GeometryCurve'.");
    return 0;
}

// constructor method
int GeometryCurvePy::PyInit(PyObject* /*args*/, PyObject* /*kwd*/)
{
    return 0;
}

PyObject* GeometryCurvePy::toShape(PyObject *args)
{
    Handle_Geom_Geometry g = getGeometryPtr()->handle();
    Handle_Geom_Curve c = Handle_Geom_Curve::DownCast(g);
    try {
        if (!c.IsNull()) {
            double u,v;
            u=c->FirstParameter();
            v=c->LastParameter();
            if (!PyArg_ParseTuple(args, "|dd", &u,&v))
                return 0;
            BRepBuilderAPI_MakeEdge mkBuilder(c, u, v);
            TopoDS_Shape sh = mkBuilder.Shape();
            return new TopoShapeEdgePy(new TopoShape(sh));
        }
    }
    catch (Standard_Failure) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        PyErr_SetString(PyExc_Exception, e->GetMessageString());
        return 0;
    }

    PyErr_SetString(PyExc_Exception, "Geometry is not a curve");
    return 0;
}

PyObject* GeometryCurvePy::discretize(PyObject *args, PyObject *kwds)
{
    try {
        Handle_Geom_Geometry g = getGeometryPtr()->handle();
        Handle_Geom_Curve c = Handle_Geom_Curve::DownCast(g);
        if (c.IsNull()) {
            PyErr_SetString(PyExc_Exception, "Geometry is not a curve");
            return 0;
        }

        GeomAdaptor_Curve adapt(c);
        bool uniformAbscissaPoints = false;
        bool uniformAbscissaDistance = false;
        int numPoints = -1;
        double distance = -1;
        double first = adapt.FirstParameter();
        double last = adapt.LastParameter();

        // use no kwds
        PyObject* dist_or_num;
        if (PyArg_ParseTuple(args, "O", &dist_or_num)) {
            if (PyInt_Check(dist_or_num)) {
                numPoints = PyInt_AsLong(dist_or_num);
                uniformAbscissaPoints = true;
            }
            else if (PyFloat_Check(dist_or_num)) {
                distance = PyFloat_AsDouble(dist_or_num);
                uniformAbscissaDistance = true;
            }
            else {
                PyErr_SetString(PyExc_TypeError, "Either int or float expected");
                return 0;
            }
        }
        else {
            // use Number kwds
            static char* kwds_numPoints[] = {"Number","First","Last",NULL};
            PyErr_Clear();
            if (PyArg_ParseTupleAndKeywords(args, kwds, "i|dd", kwds_numPoints, &numPoints, &first, &last)) {
                uniformAbscissaPoints = true;
            }
            else {
                // use Abscissa kwds
                static char* kwds_Distance[] = {"Distance","First","Last",NULL};
                PyErr_Clear();
                if (PyArg_ParseTupleAndKeywords(args, kwds, "d|dd", kwds_Distance, &distance, &first, &last)) {
                    uniformAbscissaDistance = true;
                }
            }
        }

        if (uniformAbscissaPoints || uniformAbscissaDistance) {
            GCPnts_UniformAbscissa discretizer;
            if (uniformAbscissaPoints)
                discretizer.Initialize (adapt, numPoints, first, last);
            else
                discretizer.Initialize (adapt, distance, first, last);

            if (discretizer.IsDone () && discretizer.NbPoints () > 0) {
                Py::List points;
                int nbPoints = discretizer.NbPoints ();
                for (int i=1; i<=nbPoints; i++) {
                    gp_Pnt p = adapt.Value (discretizer.Parameter (i));
                    points.append(Py::Vector(Base::Vector3d(p.X(),p.Y(),p.Z())));
                }

                return Py::new_reference_to(points);
            }
            else {
                PyErr_SetString(PyExc_Exception, "Discretization of curve failed");
                return 0;
            }
        }

        // use Deflection kwds
        static char* kwds_Deflection[] = {"Deflection","First","Last",NULL};
        PyErr_Clear();
        double deflection;
        if (PyArg_ParseTupleAndKeywords(args, kwds, "d|dd", kwds_Deflection, &deflection, &first, &last)) {
            GCPnts_UniformDeflection discretizer(adapt, deflection, first, last);
            if (discretizer.IsDone () && discretizer.NbPoints () > 0) {
                Py::List points;
                int nbPoints = discretizer.NbPoints ();
                for (int i=1; i<=nbPoints; i++) {
                    gp_Pnt p = discretizer.Value (i);
                    points.append(Py::Vector(Base::Vector3d(p.X(),p.Y(),p.Z())));
                }

                return Py::new_reference_to(points);
            }
            else {
                PyErr_SetString(PyExc_Exception, "Discretization of curve failed");
                return 0;
            }
        }

        // use TangentialDeflection kwds
        static char* kwds_TangentialDeflection[] = {"Angular","Curvature","First","Last","Minimum",NULL};
        PyErr_Clear();
        double angular;
        double curvature;
        int minimumPoints = 2;
        if (PyArg_ParseTupleAndKeywords(args, kwds, "dd|ddi", kwds_TangentialDeflection, &angular, &curvature, &first, &last, &minimumPoints)) {
            GCPnts_TangentialDeflection discretizer(adapt, first, last, angular, curvature, minimumPoints);
            if (discretizer.NbPoints () > 0) {
                Py::List points;
                int nbPoints = discretizer.NbPoints ();
                for (int i=1; i<=nbPoints; i++) {
                    gp_Pnt p = discretizer.Value (i);
                    points.append(Py::Vector(Base::Vector3d(p.X(),p.Y(),p.Z())));
                }

                return Py::new_reference_to(points);
            }
            else {
                PyErr_SetString(PyExc_Exception, "Discretization of curve failed");
                return 0;
            }
        }

        // use QuasiNumber kwds
        static char* kwds_QuasiNumPoints[] = {"QuasiNumber","First","Last",NULL};
        PyErr_Clear();
        int quasiNumPoints;
        if (PyArg_ParseTupleAndKeywords(args, kwds, "i|dd", kwds_QuasiNumPoints, &quasiNumPoints, &first, &last)) {
            GCPnts_QuasiUniformAbscissa discretizer(adapt, quasiNumPoints, first, last);
            if (discretizer.NbPoints () > 0) {
                Py::List points;
                int nbPoints = discretizer.NbPoints ();
                for (int i=1; i<=nbPoints; i++) {
                    gp_Pnt p = adapt.Value (discretizer.Parameter (i));
                    points.append(Py::Vector(Base::Vector3d(p.X(),p.Y(),p.Z())));
                }

                return Py::new_reference_to(points);
            }
            else {
                PyErr_SetString(PyExc_Exception, "Discretization of curve failed");
                return 0;
            }
        }

        // use QuasiDeflection kwds
        static char* kwds_QuasiDeflection[] = {"QuasiDeflection","First","Last",NULL};
        PyErr_Clear();
        double quasiDeflection;
        if (PyArg_ParseTupleAndKeywords(args, kwds, "d|dd", kwds_QuasiDeflection, &quasiDeflection, &first, &last)) {
            GCPnts_QuasiUniformDeflection discretizer(adapt, quasiDeflection, first, last);
            if (discretizer.NbPoints () > 0) {
                Py::List points;
                int nbPoints = discretizer.NbPoints ();
                for (int i=1; i<=nbPoints; i++) {
                    gp_Pnt p = discretizer.Value (i);
                    points.append(Py::Vector(Base::Vector3d(p.X(),p.Y(),p.Z())));
                }

                return Py::new_reference_to(points);
            }
            else {
                PyErr_SetString(PyExc_Exception, "Discretization of curve failed");
                return 0;
            }
        }
    }
    catch (const Base::Exception& e) {
        PyErr_SetString(PyExc_Exception, e.what());
        return 0;
    }

    PyErr_SetString(PyExc_Exception,"Wrong arguments");
    return 0;
}

PyObject* GeometryCurvePy::length(PyObject *args)
{
    Handle_Geom_Geometry g = getGeometryPtr()->handle();
    Handle_Geom_Curve c = Handle_Geom_Curve::DownCast(g);
    try {
        if (!c.IsNull()) {
            double u=c->FirstParameter();
            double v=c->LastParameter();
            double t=Precision::Confusion();
            if (!PyArg_ParseTuple(args, "|ddd", &u,&v,&t))
                return 0;
            GeomAdaptor_Curve adapt(c);
            double len = GCPnts_AbscissaPoint::Length(adapt,u,v,t);
            return PyFloat_FromDouble(len);
        }
    }
    catch (Standard_Failure) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        PyErr_SetString(PyExc_Exception, e->GetMessageString());
        return 0;
    }

    PyErr_SetString(PyExc_Exception, "Geometry is not a curve");
    return 0;
}

PyObject* GeometryCurvePy::value(PyObject *args)
{
    Handle_Geom_Geometry g = getGeometryPtr()->handle();
    Handle_Geom_Curve c = Handle_Geom_Curve::DownCast(g);
    try {
        if (!c.IsNull()) {
            double u;
            if (!PyArg_ParseTuple(args, "d", &u))
                return 0;
            gp_Pnt p = c->Value(u);
            return new Base::VectorPy(Base::Vector3d(p.X(),p.Y(),p.Z()));
        }
    }
    catch (Standard_Failure) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        PyErr_SetString(PyExc_Exception, e->GetMessageString());
        return 0;
    }

    PyErr_SetString(PyExc_Exception, "Geometry is not a curve");
    return 0;
}

PyObject* GeometryCurvePy::tangent(PyObject *args)
{
    Handle_Geom_Geometry g = getGeometryPtr()->handle();
    Handle_Geom_Curve c = Handle_Geom_Curve::DownCast(g);
    try {
        if (!c.IsNull()) {
            double u;
            if (!PyArg_ParseTuple(args, "d", &u))
                return 0;
            gp_Dir dir;
            Py::Tuple tuple(1);
            GeomLProp_CLProps prop(c,u,1,Precision::Confusion());
            if (prop.IsTangentDefined()) {
                prop.Tangent(dir);
                tuple.setItem(0, Py::Vector(Base::Vector3d(dir.X(),dir.Y(),dir.Z())));
            }

            return Py::new_reference_to(tuple);
        }
    }
    catch (Standard_Failure) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        PyErr_SetString(PyExc_Exception, e->GetMessageString());
        return 0;
    }

    PyErr_SetString(PyExc_Exception, "Geometry is not a curve");
    return 0;
}

PyObject* GeometryCurvePy::parameter(PyObject *args)
{
    Handle_Geom_Geometry g = getGeometryPtr()->handle();
    Handle_Geom_Curve c = Handle_Geom_Curve::DownCast(g);
    try {
        if (!c.IsNull()) {
            PyObject *p;
            if (!PyArg_ParseTuple(args, "O!", &(Base::VectorPy::Type), &p))
                return 0;
            Base::Vector3d v = Py::Vector(p, false).toVector();
            gp_Pnt pnt(v.x,v.y,v.z);
            GeomAPI_ProjectPointOnCurve ppc(pnt, c);
            double val = ppc.LowerDistanceParameter();
            return Py::new_reference_to(Py::Float(val));
        }
    }
    catch (Standard_Failure) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        PyErr_SetString(PyExc_Exception, e->GetMessageString());
        return 0;
    }

    PyErr_SetString(PyExc_Exception, "Geometry is not a curve");
    return 0;
}

PyObject* GeometryCurvePy::makeRuledSurface(PyObject *args)
{
    PyObject* curve;
    if (!PyArg_ParseTuple(args, "O!", &(Part::GeometryCurvePy::Type), &curve))
        return 0;

    try {
        Handle_Geom_Curve aCrv1 = Handle_Geom_Curve::DownCast(getGeometryPtr()->handle());
        GeometryCurvePy* c = static_cast<GeometryCurvePy*>(curve);
        Handle_Geom_Curve aCrv2 = Handle_Geom_Curve::DownCast(c->getGeometryPtr()->handle());
        Handle_Geom_Surface aSurf = GeomFill::Surface (aCrv1, aCrv2);
        if (aSurf.IsNull()) {
            PyErr_SetString(PyExc_Exception, "Failed to create ruled surface");
            return 0;
        }
        // check the result surface type
        if (aSurf->IsKind(STANDARD_TYPE(Geom_RectangularTrimmedSurface))) {
            Handle_Geom_RectangularTrimmedSurface aTSurf = 
                Handle_Geom_RectangularTrimmedSurface::DownCast(aSurf);
            return new RectangularTrimmedSurfacePy(new GeomTrimmedSurface(aTSurf));
        }
        else if (aSurf->IsKind(STANDARD_TYPE(Geom_BSplineSurface))) {
            Handle_Geom_BSplineSurface aBSurf = 
                Handle_Geom_BSplineSurface::DownCast(aSurf);
            return new BSplineSurfacePy(new GeomBSplineSurface(aBSurf));
        }
        else {
            PyErr_Format(PyExc_NotImplementedError, "Ruled surface is of type '%s'",
                aSurf->DynamicType()->Name());
            return 0;
        }
    }
    catch (Standard_Failure) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        PyErr_SetString(PyExc_Exception, e->GetMessageString());
        return 0;
    }

    PyErr_SetString(PyExc_Exception, "Geometry is not a curve");
    return 0;
}

PyObject* GeometryCurvePy::intersect2d(PyObject *args)
{
    PyObject *c,*p;
    if (!PyArg_ParseTuple(args, "O!O!", &(Part::GeometryCurvePy::Type), &c,
                                        &(Part::PlanePy::Type), &p))
        return 0;

    try {
        Handle_Geom_Curve self = Handle_Geom_Curve::DownCast(getGeometryPtr()->handle());
        Handle_Geom_Curve curv = Handle_Geom_Curve::DownCast(static_cast<GeometryPy*>(c)->
            getGeometryPtr()->handle());
        Handle_Geom_Plane plane = Handle_Geom_Plane::DownCast(static_cast<GeometryPy*>(p)->
            getGeometryPtr()->handle());

        Handle_Geom2d_Curve curv1 = GeomAPI::To2d(self, plane->Pln());
        Handle_Geom2d_Curve curv2 = GeomAPI::To2d(curv, plane->Pln());
        Geom2dAPI_InterCurveCurve intCC(curv1, curv2);
        int nbPoints = intCC.NbPoints();
        Py::List list;
        for (int i=1; i<= nbPoints; i++) {
            gp_Pnt2d pt = intCC.Point(i);
            Py::Tuple tuple(2);
            tuple.setItem(0, Py::Float(pt.X()));
            tuple.setItem(1, Py::Float(pt.Y()));
            list.append(tuple);
        }
        return Py::new_reference_to(list);
    }
    catch (Standard_Failure) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        PyErr_SetString(PyExc_Exception, e->GetMessageString());
        return 0;
    }
}

PyObject* GeometryCurvePy::toBSpline(PyObject * args)
{
    Handle_Geom_Geometry g = getGeometryPtr()->handle();
    Handle_Geom_Curve c = Handle_Geom_Curve::DownCast(g);
    try {
        if (!c.IsNull()) {
            double u,v;
            u=c->FirstParameter();
            v=c->LastParameter();
            if (!PyArg_ParseTuple(args, "|dd", &u,&v))
                return 0;
            ShapeConstruct_Curve scc;
            Handle_Geom_BSplineCurve spline = scc.ConvertToBSpline(c, u, v, Precision::Confusion());
            if (spline.IsNull())
                Standard_NullValue::Raise("Conversion to B-Spline failed");
            return new BSplineCurvePy(new GeomBSplineCurve(spline));
        }
    }
    catch (Standard_Failure) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        PyErr_SetString(PyExc_Exception, e->GetMessageString());
        return 0;
    }

    PyErr_SetString(PyExc_Exception, "Geometry is not a curve");
    return 0;
}

PyObject* GeometryCurvePy::approximateBSpline(PyObject *args)
{
    double tolerance;
    int maxSegment, maxDegree;
    char* order = "C2";
    if (!PyArg_ParseTuple(args, "dii|s", &tolerance, &maxSegment, &maxDegree, &order))
        return 0;

    GeomAbs_Shape absShape;
    std::string str = order;
    if (str == "C0")
        absShape = GeomAbs_C0;
    else if (str == "G1")
        absShape = GeomAbs_G1;
    else if (str == "C1")
        absShape = GeomAbs_C1;
    else if (str == "G2")
        absShape = GeomAbs_G2;
    else if (str == "C2")
        absShape = GeomAbs_C2;
    else if (str == "C3")
        absShape = GeomAbs_C3;
    else if (str == "CN")
        absShape = GeomAbs_CN;
    else
        absShape = GeomAbs_C2;

    try {
        Handle_Geom_Curve self = Handle_Geom_Curve::DownCast(getGeometryPtr()->handle());
        GeomConvert_ApproxCurve approx(self, tolerance, absShape, maxSegment, maxDegree);
        if (approx.IsDone()) {
            return new BSplineCurvePy(new GeomBSplineCurve(approx.Curve()));
        }
        else if (approx.HasResult()) {
            std::stringstream str;
            str << "Maximum error (" << approx.MaxError() << ") is outside tolerance";
            PyErr_SetString(PyExc_RuntimeError, str.str().c_str());
            return 0;
        }
        else {
            PyErr_SetString(PyExc_RuntimeError, "Approximation of curve failed");
            return 0;
        }
    }
    catch (Standard_Failure) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        PyErr_SetString(PyExc_Exception, e->GetMessageString());
        return 0;
    }
}

Py::String GeometryCurvePy::getContinuity(void) const
{
    GeomAbs_Shape c = Handle_Geom_Curve::DownCast
        (getGeometryPtr()->handle())->Continuity();
    std::string str;
    switch (c) {
    case GeomAbs_C0:
        str = "C0";
        break;
    case GeomAbs_G1:
        str = "G1";
        break;
    case GeomAbs_C1:
        str = "C1";
        break;
    case GeomAbs_G2:
        str = "G2";
        break;
    case GeomAbs_C2:
        str = "C2";
        break;
    case GeomAbs_C3:
        str = "C3";
        break;
    case GeomAbs_CN:
        str = "CN";
        break;
    default:
        str = "Unknown";
        break;
    }
    return Py::String(str);
}

Py::Float GeometryCurvePy::getFirstParameter(void) const
{
    return Py::Float(Handle_Geom_Curve::DownCast
        (getGeometryPtr()->handle())->FirstParameter());
}

Py::Float GeometryCurvePy::getLastParameter(void) const
{
    return Py::Float(Handle_Geom_Curve::DownCast
        (getGeometryPtr()->handle())->LastParameter());
}

PyObject *GeometryCurvePy::getCustomAttributes(const char* /*attr*/) const
{
    return 0;
}

int GeometryCurvePy::setCustomAttributes(const char* /*attr*/, PyObject* /*obj*/)
{
    return 0; 
}

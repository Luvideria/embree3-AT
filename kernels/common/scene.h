// ======================================================================== //
// Copyright 2009-2020 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //
 
#pragma once

#include "default.h"
#include "device.h"
#include "builder.h"
#include "../../common/algorithms/parallel_any_of.h"
#include "scene_triangle_mesh.h"
#include "scene_quad_mesh.h"
#include "scene_user_geometry.h"
#include "scene_instance.h"
#include "scene_curves.h"
#include "scene_line_segments.h"
#include "scene_subdiv_mesh.h"
#include "scene_grid_mesh.h"
#include "scene_points.h"
#include "../subdiv/tessellation_cache.h"

#include "acceln.h"
#include "geometry.h"

namespace embree
{
  /*! Base class all scenes are derived from */
  class Scene : public AccelN
  {
    ALIGNED_CLASS_(16);

  public:
    template<typename Ty, bool mblur = false>
      class Iterator
      {
      public:
      Iterator ()  {}
      
      Iterator (Scene* scene, bool all = false) 
      : scene(scene), all(all) {}
      
      __forceinline Ty* at(const size_t i)
      {
        Geometry* geom = scene->geometries[i].ptr;
        if (geom == nullptr) return nullptr;
        if (!all && !geom->isEnabled()) return nullptr;
        const size_t mask = geom->getTypeMask() & Ty::geom_type; 
        if (!(mask)) return nullptr;
        if ((geom->numTimeSteps != 1) != mblur) return nullptr;
        return (Ty*) geom;
      }

      __forceinline Ty* operator[] (const size_t i) {
        return at(i);
      }

      __forceinline size_t size() const {
        return scene->size();
      }
      
      __forceinline size_t numPrimitives() const {
        return scene->getNumPrimitives(Ty::geom_type,mblur);
      }

      __forceinline size_t maxPrimitivesPerGeometry() 
      {
        size_t ret = 0;
        for (size_t i=0; i<scene->size(); i++) {
          Ty* mesh = at(i);
          if (mesh == nullptr) continue;
          ret = max(ret,mesh->size());
        }
        return ret;
      }

      __forceinline unsigned int maxGeomID() 
      {
        unsigned int ret = 0;
        for (size_t i=0; i<scene->size(); i++) {
          Ty* mesh = at(i);
          if (mesh == nullptr) continue;
          ret = max(ret,(unsigned int)i);
        }
        return ret;
      }

      __forceinline unsigned maxTimeStepsPerGeometry()
      {
        unsigned ret = 0;
        for (size_t i=0; i<scene->size(); i++) {
          Ty* mesh = at(i);
          if (mesh == nullptr) continue;
          ret = max(ret,mesh->numTimeSteps);
        }
        return ret;
      }
      
    private:
      Scene* scene;
      bool all;
      };

      class Iterator2
      {
      public:
      Iterator2 () {}
      
      Iterator2 (Scene* scene, Geometry::GTypeMask typemask, bool mblur) 
      : scene(scene), typemask(typemask), mblur(mblur) {}
      
      __forceinline Geometry* at(const size_t i)
      {
        Geometry* geom = scene->geometries[i].ptr;
        if (geom == nullptr) return nullptr;
        if (!geom->isEnabled()) return nullptr;
        if (!(geom->getTypeMask() & typemask)) return nullptr;
        if ((geom->numTimeSteps != 1) != mblur) return nullptr;
        return geom;
      }

      __forceinline Geometry* operator[] (const size_t i) {
        return at(i);
      }

      __forceinline size_t size() const {
        return scene->size();
      }
      
    private:
      Scene* scene;
      Geometry::GTypeMask typemask;
      bool mblur;
    };

  public:
    
    /*! Scene construction */
    Scene (Device* device);

    /*! Scene destruction */
    ~Scene ();

  private:
    /*! class is non-copyable */
    Scene (const Scene& other) DELETED; // do not implement
    Scene& operator= (const Scene& other) DELETED; // do not implement

  public:
    void createTriangleAccel();
    void createTriangleMBAccel();
    void createQuadAccel();
    void createQuadMBAccel();
    void createHairAccel();
    void createHairMBAccel();
    void createSubdivAccel();
    void createSubdivMBAccel();
    void createUserGeometryAccel();
    void createUserGeometryMBAccel();
    void createInstanceAccel();
    void createInstanceMBAccel();
    void createInstanceExpensiveAccel();
    void createInstanceExpensiveMBAccel();
    void createGridAccel();
    void createGridMBAccel();

    /*! prints statistics about the scene */
    void printStatistics();

    /*! clears the scene */
    void clear();

    /*! detaches some geometry */
    void detachGeometry(size_t geomID);

    void setBuildQuality(RTCBuildQuality quality_flags);
    RTCBuildQuality getBuildQuality() const;
    
    void setSceneFlags(RTCSceneFlags scene_flags);
    RTCSceneFlags getSceneFlags() const;
    
    void commit (bool join);
    void commit_task ();
    void build () {}

    void updateInterface();

    /* return number of geometries */
    __forceinline size_t size() const { return geometries.size(); }
    
    /* bind geometry to the scene */
    unsigned int bind (unsigned geomID, Ref<Geometry> geometry);
    
    /* determines if scene is modified */
    __forceinline bool isModified() const { return modified; }

    /* sets modified flag */
    __forceinline void setModified(bool f = true) { 
      modified = f; 
    }

    __forceinline bool isGeometryModified(size_t geomID)
    {
      Ref<Geometry>& g = geometries[geomID];
      if (!g) return false;
      if (!g->isEnabled()) return false;
      return g->getModCounter() > geometryModCounters_[geomID];
    }

  protected:
    
    __forceinline void checkIfModifiedAndSet () 
    {
      if (isModified ()) return;
      
      auto geometryIsModified = [this](size_t geomID)->bool {
        return isGeometryModified(geomID);
      };

      if (parallel_any_of (size_t(0), geometries.size (), geometryIsModified)) {
        setModified ();
      }
    }
    
  public:

    /* get mesh by ID */
    __forceinline       Geometry* get(size_t i)       { assert(i < geometries.size()); return geometries[i].ptr; }
    __forceinline const Geometry* get(size_t i) const { assert(i < geometries.size()); return geometries[i].ptr; }

    template<typename Mesh>
      __forceinline       Mesh* get(size_t i)       { 
      assert(i < geometries.size()); 
      assert(geometries[i]->getTypeMask() & Mesh::geom_type);
      return (Mesh*)geometries[i].ptr; 
    }
    template<typename Mesh>
      __forceinline const Mesh* get(size_t i) const { 
      assert(i < geometries.size()); 
      assert(geometries[i]->getTypeMask() & Mesh::geom_type);
      return (Mesh*)geometries[i].ptr; 
    }

    template<typename Mesh>
    __forceinline Mesh* getSafe(size_t i) {
      assert(i < geometries.size());
      if (geometries[i] == null) return nullptr;
      if (!(geometries[i]->getTypeMask() & Mesh::geom_type)) return nullptr;
      else return (Mesh*) geometries[i].ptr;
    }

    __forceinline Ref<Geometry> get_locked(size_t i)  {
      Lock<SpinLock> lock(geometriesMutex);
      assert(i < geometries.size()); 
      return geometries[i]; 
    }

    /* flag decoding */
    __forceinline bool isFastAccel() const { return !isCompactAccel() && !isRobustAccel(); }
    __forceinline bool isCompactAccel() const { return scene_flags & RTC_SCENE_FLAG_COMPACT; }
    __forceinline bool isRobustAccel()  const { return scene_flags & RTC_SCENE_FLAG_ROBUST; }
    __forceinline bool isStaticAccel()  const { return !(scene_flags & RTC_SCENE_FLAG_DYNAMIC); }
    __forceinline bool isDynamicAccel() const { return scene_flags & RTC_SCENE_FLAG_DYNAMIC; }
    
    __forceinline bool hasContextFilterFunction() const {
      return scene_flags & RTC_SCENE_FLAG_CONTEXT_FILTER_FUNCTION;
    }
    
    __forceinline bool hasGeometryFilterFunction() {
      return world.numFilterFunctions != 0;
    }
      
    __forceinline bool hasFilterFunction() {
      return hasContextFilterFunction() || hasGeometryFilterFunction();
    }
    
    /* test if scene got already build */
    __forceinline bool isBuild() const { return is_build; }

  public:
    IDPool<unsigned,0xFFFFFFFE> id_pool;
    vector<Ref<Geometry>> geometries; //!< list of all user geometries
    vector<unsigned int> geometryModCounters_;
    vector<float*> vertices;
    
  public:
    Device* device;

    /* these are to detect if we need to recreate the acceleration structures */
    bool flags_modified;
    unsigned int enabled_geometry_types;
    
    RTCSceneFlags scene_flags;
    RTCBuildQuality quality_flags;
    RTCBuildArguments buildArguments;
    MutexSys buildMutex;
    SpinLock geometriesMutex;
    bool is_build;
  
    bool modified;                   //!< true if scene got modified    
  public:
    
    /*! global lock step task scheduler */
#if defined(TASKING_INTERNAL) 
    MutexSys schedulerMutex;
    Ref<TaskScheduler> scheduler;
#elif defined(TASKING_TBB) && TASKING_TBB_USE_TASK_ISOLATION
    tbb::isolated_task_group* group;
#elif defined(TASKING_TBB)
    tbb::task_group* group;
#elif defined(TASKING_PPL)
    concurrency::task_group* group;
#endif
    
  public:
    struct BuildProgressMonitorInterface : public BuildProgressMonitor {
      BuildProgressMonitorInterface(Scene* scene) 
      : scene(scene) {}
      void operator() (size_t dn) const { scene->progressMonitor(double(dn)); }
    private:
      Scene* scene;
    };
    BuildProgressMonitorInterface progressInterface;
    RTCProgressMonitorFunction progress_monitor_function;
    void* progress_monitor_ptr;
    std::atomic<size_t> progress_monitor_counter;
    void progressMonitor(double nprims);
    void setProgressMonitorFunction(RTCProgressMonitorFunction func, void* ptr);

  public:
    
  private:

    GeometryCounts world;               //!< counts for geometry

  public:

    __forceinline size_t numPrimitives() const {
      return world.size();
    }

    __forceinline size_t getNumPrimitives(Geometry::GTypeMask mask, bool mblur) const
    {
      size_t count = 0;
      
      if (mask & Geometry::MTY_TRIANGLE_MESH)
        count += mblur ? world.numMBTriangles : world.numTriangles;
      
      if (mask & Geometry::MTY_QUAD_MESH)
        count += mblur ? world.numMBQuads : world.numQuads;
      
      if (mask & Geometry::MTY_CURVE2)
        count += mblur ? world.numMBLineSegments : world.numLineSegments;
      
      if (mask & Geometry::MTY_CURVE4)
        count += mblur ? world.numMBBezierCurves : world.numBezierCurves;
      
      if (mask & Geometry::MTY_POINTS)
        count += mblur ? world.numMBPoints : world.numPoints;
      
      if (mask & Geometry::MTY_SUBDIV_MESH)
        count += mblur ? world.numMBSubdivPatches : world.numSubdivPatches;
      
      if (mask & Geometry::MTY_USER_GEOMETRY)
        count += mblur ? world.numMBUserGeometries : world.numUserGeometries;
      
      if (mask & Geometry::MTY_INSTANCE_CHEAP)
        count += mblur ? world.numMBInstancesCheap : world.numInstancesCheap;
      
      if (mask & Geometry::MTY_INSTANCE_EXPENSIVE)
        count += mblur  ? world.numMBInstancesExpensive : world.numInstancesExpensive;
      
      if (mask & Geometry::MTY_GRID_MESH)
        count += mblur  ? world.numMBGrids : world.numGrids;
      
      return count;
    }
    
    template<typename Mesh, bool mblur>
    __forceinline unsigned getNumTimeSteps()
    {
      if (!mblur)
        return 1;

      Scene::Iterator<Mesh,mblur> iter(this);
      return iter.maxTimeStepsPerGeometry();
    }

    template<typename Mesh, bool mblur>
    __forceinline unsigned int getMaxGeomID()
    {
      Scene::Iterator<Mesh,mblur> iter(this);
      return iter.maxGeomID();
    }
  };
}

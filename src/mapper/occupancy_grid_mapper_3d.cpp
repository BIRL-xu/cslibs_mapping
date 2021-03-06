#include "occupancy_grid_mapper_3d.h"

#include <cslibs_plugins_data/types/pointcloud_3d.hpp>
#include <cslibs_math_3d/linear/pointcloud.hpp>
#include <cslibs_math_ros/tf/conversion_3d.hpp>
#include <cslibs_time/time.hpp>
#include <cslibs_ndt/serialization/filesystem.hpp>

#include <class_loader/class_loader_register_macro.h>
CLASS_LOADER_REGISTER_CLASS(cslibs_mapping::mapper::OccupancyGridMapper3D, cslibs_mapping::mapper::Mapper)

namespace cslibs_mapping {
namespace mapper {
const OccupancyGridMapper3D::map_t::ConstPtr OccupancyGridMapper3D::getMap() const
{
    map_->get()->updateInnerOccupancy();
    return map_;
}

bool OccupancyGridMapper3D::setupMap(ros::NodeHandle &nh)
{
    auto param_name = [this](const std::string &name){return name_ + "/" + name;};

    const double resolution = nh.param<double>(param_name("resolution"), 1.0);
    map_.reset(new maps::OccupancyGridMap3D(map_frame_, resolution));
    return true;
}

bool OccupancyGridMapper3D::uses(const data_t::ConstPtr &type)
{
    return type->isType<cslibs_plugins_data::types::Pointcloud3d>();
}

void OccupancyGridMapper3D::process(const data_t::ConstPtr &data)
{
    assert (uses(data));

    const cslibs_plugins_data::types::Pointcloud3d &cloud_data = data->as<cslibs_plugins_data::types::Pointcloud3d>();

    tf::Transform o_T_d_tmp;
    if (tf_->lookupTransform(map_frame_,
                             cloud_data.frame(),
                             ros::Time(cloud_data.timeFrame().start.seconds()),
                             o_T_d_tmp,
                             tf_timeout_)) {
        cslibs_math_3d::Transform3d o_T_d = cslibs_math_ros::tf::conversion_3d::from(o_T_d_tmp);

        const cslibs_math_3d::Pointcloud3d::ConstPtr &points = cloud_data.points();
        if (points) {
            octomap::Pointcloud cloud;

            for (const auto &point : *points) {
                if (point.isNormal()) {
                    const cslibs_math_3d::Point3d map_point = o_T_d * point;
                    if (map_point.isNormal())
                        cloud.push_back(map_point(0), map_point(1), map_point(2));
                }
            }
            const octomath::Vector3 origin(o_T_d.translation()(0),
                                           o_T_d.translation()(1),
                                           o_T_d.translation()(2));

            map_->get()->insertPointCloud(cloud, origin, -1, true, true);
        }
    }
}

bool OccupancyGridMapper3D::saveMap()
{
    if (!map_) {
        std::cout << "[OccupancyGridMapper3D '" << name_ << "']: No Map." << std::endl;
        return true;
    }

    std::cout << "[OccupancyGridMapper3D '" << name_ << "']: Saving Map..." << std::endl;
    if (!checkPath()) {
        std::cout << "[OccupancyGridMapper3D '" << name_ << "']: '" << path_ << "' is not a directory." << std::endl;
        return false;
    }

    using path_t = boost::filesystem::path;
    path_t path_root(path_);
    if (!cslibs_ndt::common::serialization::create_directory(path_root))
        return false;

    std::string map_path_yaml = (path_ / boost::filesystem::path("map.ot")).string();
    {
        std::ofstream map_out_yaml(map_path_yaml);
        if (!map_out_yaml.is_open()) {
            std::cout << "[OccupancyGridMapper3D '" << name_ << "']: Could not open file '" << map_path_yaml << "'." << std::endl;
            return false;
        }
        if (map_->get()->write(map_out_yaml)) {
            map_out_yaml.close();
            return true;
        }
        else {
            std::cout << "[OccupancyGridMapper3D '" << name_ << "']: Could not write to file '" << map_path_yaml << "'." << std::endl;
            return false;
        }
    }
    return true;
}
}
}

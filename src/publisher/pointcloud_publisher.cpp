#include "pointcloud_publisher.h"

#include <cslibs_mapping/maps/ndt_grid_map_3d.hpp>
#include <cslibs_mapping/maps/occupancy_ndt_grid_map_3d.hpp>

#include <cslibs_ndt_3d/conversion/sensor_msgs_pointcloud2.hpp>

#include <class_loader/class_loader_register_macro.h>
CLASS_LOADER_REGISTER_CLASS(cslibs_mapping::publisher::PointcloudPublisher, cslibs_mapping::publisher::Publisher)

namespace cslibs_mapping {
namespace publisher {
bool PointcloudPublisher::uses(const map_t::ConstPtr &map) const
{
    return map->isType<cslibs_mapping::maps::NDTGridMap3D>() ||
           map->isType<cslibs_mapping::maps::OccupancyNDTGridMap3D>();
}

void PointcloudPublisher::doAdvertise(ros::NodeHandle &nh, const std::string &topic)
{
    auto param_name = [this](const std::string &name){return name_ + "/" + name;};

    const bool occupancy_ndt = nh.param<bool>(param_name("occupancy_ndt"), false);
    if (occupancy_ndt) {
        const double prob_prior    = nh.param(param_name("prob_prior"),    0.5);
        const double prob_free     = nh.param(param_name("prob_free"),     0.45);
        const double prob_occupied = nh.param(param_name("prob_occupied"), 0.65);
        ivm_.reset(new cslibs_gridmaps::utility::InverseModel(
                       prob_prior, prob_free, prob_occupied));

        occ_threshold_ = nh.param<double>(param_name("occ_threshold"), 0.169);
    }

    publisher_ = nh.advertise<sensor_msgs::PointCloud2>(topic, 1);
}

void PointcloudPublisher::doPublish(const map_t::ConstPtr &map, const ros::Time &time)
{
    if (map->isType<cslibs_mapping::maps::NDTGridMap3D>())
        return publishNDTGridMap3D(map, time);
    if (map->isType<cslibs_mapping::maps::OccupancyNDTGridMap3D>())
        return publishOccupancyNDTGridMap3D(map, time);
    std::cout << "[PointcloudPublisher '" << name_ << "']: Got wrong map type!" << std::endl;
}

void PointcloudPublisher::publishNDTGridMap3D(const map_t::ConstPtr &map, const ros::Time &time)
{
    using local_map_t = cslibs_ndt_3d::dynamic_maps::Gridmap;
    const local_map_t::Ptr m = map->as<cslibs_mapping::maps::NDTGridMap3D>().get();
    if (m) {
        sensor_msgs::PointCloud2 msg;
        cslibs_ndt_3d::conversion::from(m, msg);

        msg.header.stamp    = time;
        msg.header.frame_id = map->getFrame();

        publisher_.publish(msg);
        return;
    }
    std::cout << "[PointcloudPublisher '" << name_ << "']: Map could not be published!" << std::endl;
}

void PointcloudPublisher::publishOccupancyNDTGridMap3D(const map_t::ConstPtr &map, const ros::Time &time)
{
    if (ivm_) {
        using local_map_t = cslibs_ndt_3d::dynamic_maps::OccupancyGridmap;
        const local_map_t::Ptr m = map->as<cslibs_mapping::maps::OccupancyNDTGridMap3D>().get();
        if (m) {
            sensor_msgs::PointCloud2 msg;
            cslibs_ndt_3d::conversion::from(m, msg, ivm_, occ_threshold_);

            msg.header.stamp    = time;
            msg.header.frame_id = map->getFrame();

            publisher_.publish(msg);
            return;
        }
    }
    std::cout << "[PointcloudPublisher '" << name_ << "']: Map could not be published!" << std::endl;
}
}
}

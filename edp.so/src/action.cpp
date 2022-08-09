/********************************************************************************
 *
 * Copyright (c) 2018-2022 ROCm Developer Tools
 *
 * MIT LICENSE:
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/
#include "include/action.h"

#include <string>
#include <vector>
#include <iostream>
#include <regex>
#include <utility>
#include <algorithm>
#include <map>

#define __HIP_PLATFORM_HCC__
#include "hip/hip_runtime.h"
#include "hip/hip_runtime_api.h"

#include "include/rvs_key_def.h"
#include "include/edp_worker.h"
#include "include/gpu_util.h"
#include "include/rvs_util.h"
#include "include/rvsactionbase.h"
#include "include/rvsloglp.h"

extern "C" {
  #include <pci/pci.h>
  #include <linux/pci.h>
}

using std::string;
using std::vector;
using std::map;
using std::regex;

#define RVS_CONF_RAMP_INTERVAL_KEY      "ramp_interval"
#define RVS_CONF_LOG_INTERVAL_KEY       "log_interval"
#define RVS_CONF_MAX_VIOLATIONS_KEY     "max_violations"
#define RVS_CONF_COPY_MATRIX_KEY        "copy_matrix"
#define RVS_CONF_TARGET_STRESS_KEY      "target_stress"
#define RVS_CONF_TOLERANCE_KEY          "tolerance"
#define RVS_CONF_HOT_CALLS              "hot_calls"
#define RVS_CONF_MATRIX_SIZE_KEYA       "matrix_size_a"
#define RVS_CONF_MATRIX_SIZE_KEYB       "matrix_size_b"
#define RVS_CONF_MATRIX_SIZE_KEYC       "matrix_size_b"
#define RVS_CONF_EDP_OPS_TYPE           "ops_type"
#define RVS_CONF_TRANS_A                "transa"
#define RVS_CONF_TRANS_B                "transb"
#define RVS_CONF_ALPHA_VAL              "alpha"
#define RVS_CONF_BETA_VAL               "beta"
#define RVS_CONF_LDA_OFFSET             "lda"
#define RVS_CONF_LDB_OFFSET             "ldb"
#define RVS_CONF_LDC_OFFSET             "ldc"
#define RVS_CONF_HALT_WAVES             "halt_wave_timer"
#define RVS_CONF_ITERATIONS             "wave_iterations"
#define RVS_CONF_RESTART_WAVE_TIMER     "restart_wave_timer"
#define RVS_CONF_BROADCAST_WAVE         "broadcast"

#define MODULE_NAME                     "edp"
#define MODULE_NAME_CAPS                "EDP"

#define EDP_DEFAULT_RAMP_INTERVAL       5000
#define EDP_DEFAULT_LOG_INTERVAL        1000
#define EDP_DEFAULT_MAX_VIOLATIONS      0
#define EDP_DEFAULT_TOLERANCE           0.1
#define EDP_DEFAULT_COPY_MATRIX         true
#define EDP_DEFAULT_MATRIX_SIZE         5760
#define EDP_DEFAULT_HOT_CALLS           0
#define EDP_DEFAULT_TRANS_A             0
#define EDP_DEFAULT_TRANS_B             1
#define EDP_DEFAULT_ALPHA_VAL           1
#define EDP_DEFAULT_BETA_VAL            1
#define EDP_DEFAULT_LDA_OFFSET          0
#define EDP_DEFAULT_LDB_OFFSET          0
#define EDP_DEFAULT_LDC_OFFSET          0
#define EDP_DEFAULT_HALT_WAVES          1000
#define EDP_DEFAULT_WAVE_ITERATIONS     10000
#define EDP_DEFAULT_RESTART_WAVE_TIMER  0
#define EDP_DEFAULT_BROADCAST_WAVE      false

#define RVS_DEFAULT_PARALLEL            false
#define RVS_DEFAULT_DURATION            0

#define EDP_NO_COMPATIBLE_GPUS          "No AMD compatible GPU found!"

#define FLOATING_POINT_REGEX            "^[0-9]*\\.?[0-9]+$"

#define JSON_CREATE_NODE_ERROR          "JSON cannot create node"
#define EDP_DEFAULT_OPS_TYPE            "sgemm"

/**
 * @brief default class constructor
 */
edp_action::edp_action() {
    bjson = false;
}

/**
 * @brief class destructor
 */
edp_action::~edp_action() {
    property.clear();
}


/**
 * @brief runs the EDP test stress session
 * @param edp_gpus_device_index <gpu_index, gpu_id> map
 * @return true if no error occured, false otherwise
 */
bool edp_action::do_gpu_stress_test(map<int, uint16_t> edp_gpus_device_index) {
    size_t k = 0;
    for (;;) {
        unsigned int i = 0;
        if (property_wait != 0)  // delay edp execution
            sleep(property_wait);

        vector<EDPWorker> workers(edp_gpus_device_index.size());

        map<int, uint16_t>::iterator it;

        // all worker instances have the same json settings
        EDPWorker::set_use_json(bjson);

        for (it = edp_gpus_device_index.begin();
                it != edp_gpus_device_index.end(); ++it) {
            // set worker thread stress test params
            workers[i].set_name(action_name);
            workers[i].set_gpu_id(it->second);
            workers[i].set_gpu_device_index(it->first);
            workers[i].set_run_wait_ms(property_wait);
            workers[i].set_run_duration_ms(property_duration);
            workers[i].set_ramp_interval(edp_ramp_interval);
            workers[i].set_log_interval(property_log_interval);
            workers[i].set_max_violations(edp_max_violations);
            workers[i].set_copy_matrix(edp_copy_matrix);
            workers[i].set_target_stress(edp_target_stress);
            workers[i].set_tolerance(edp_tolerance);
            workers[i].set_edp_hot_calls(edp_hot_calls);
            workers[i].set_matrix_size_a(edp_matrix_size_a);
            workers[i].set_matrix_size_b(edp_matrix_size_b);
            workers[i].set_matrix_size_c(edp_matrix_size_c);
            workers[i].set_edp_ops_type(edp_ops_type);
            workers[i].set_matrix_transpose_a(edp_trans_a);
            workers[i].set_matrix_transpose_b(edp_trans_b);
            workers[i].set_alpha_val(edp_alpha_val);
            workers[i].set_beta_val(edp_beta_val);
            workers[i].set_lda_offset(edp_lda_offset);
            workers[i].set_ldb_offset(edp_ldb_offset);
            workers[i].set_ldc_offset(edp_ldc_offset);
            workers[i].set_wave_timer(edp_wave_iterations);
            workers[i].set_halt_timer(edp_halt_timer);
            workers[i].set_restart_wave_timer(edp_restart_wave_timer);

            i++;
        }

        if (property_parallel) {
            for (i = 0; i < edp_gpus_device_index.size(); i++)
                workers[i].start();

            // join threads
            for (i = 0; i < edp_gpus_device_index.size(); i++)
                workers[i].join();
        } else {
            for (i = 0; i < edp_gpus_device_index.size(); i++) {
                workers[i].start();
                workers[i].join();

                // check if stop signal was received
                if (rvs::lp::Stopping())
                    return false;
            }
        }

        // check if stop signal was received
        if (rvs::lp::Stopping())
            return false;

        if (property_count != 0) {
            k++;
            if (k == property_count)
                break;
        }
    }

    return rvs::lp::Stopping() ? false : true;
}

/**
 * @brief reads all EDP-related configuration keys from
 * the module's properties collection
 * @return true if no fatal error occured, false otherwise
 */
bool edp_action::get_all_edp_config_keys(void) {
    int error;
    string msg, ststress;
    bool bsts = true;

    if ((error =
      property_get(RVS_CONF_TARGET_STRESS_KEY, &edp_target_stress))) {
      switch (error) {  // <target_stress> is mandatory => EDP cannot continue
        case 1:
          msg = "invalid '" + std::string(RVS_CONF_TARGET_STRESS_KEY) +
              "' key value " + ststress;
          rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
          break;

        case 2:
          msg = "key '" + std::string(RVS_CONF_TARGET_STRESS_KEY) +
          "' was not found";
          rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
      }
      bsts = false;
    }

    if (property_get_int<uint64_t>(RVS_CONF_RAMP_INTERVAL_KEY,
      &edp_ramp_interval, EDP_DEFAULT_RAMP_INTERVAL)) {
        msg = "invalid '" +
        std::string(RVS_CONF_RAMP_INTERVAL_KEY) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    if (property_get_int<uint64_t>(RVS_CONF_LOG_INTERVAL_KEY,
      &property_log_interval, EDP_DEFAULT_LOG_INTERVAL)) {
        msg = "invalid '" +
        std::string(RVS_CONF_LOG_INTERVAL_KEY) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    if (property_get_int<int>(RVS_CONF_MAX_VIOLATIONS_KEY, &edp_max_violations,
     EDP_DEFAULT_MAX_VIOLATIONS)) {
        msg = "invalid '" +
        std::string(RVS_CONF_MAX_VIOLATIONS_KEY) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    if (property_get(RVS_CONF_COPY_MATRIX_KEY, &edp_copy_matrix,
      EDP_DEFAULT_COPY_MATRIX)) {
        msg = "invalid '" +
        std::string(RVS_CONF_COPY_MATRIX_KEY) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    if (property_get<float>(RVS_CONF_TOLERANCE_KEY, &edp_tolerance,
      EDP_DEFAULT_TOLERANCE)) {
        msg = "invalid '" +
        std::string(RVS_CONF_TOLERANCE_KEY) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    if (property_get<std::string>(RVS_CONF_EDP_OPS_TYPE, &edp_ops_type,
            EDP_DEFAULT_OPS_TYPE)) {
         msg = "invalid '" +
         std::string(RVS_CONF_EDP_OPS_TYPE) + "' key value";
         rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
         bsts = false;
    }

    error = property_get_int<uint64_t>(RVS_CONF_HOT_CALLS, &edp_hot_calls, EDP_DEFAULT_HOT_CALLS);
    if (error == 1) {
        msg = "invalid '" +
        std::string(RVS_CONF_HOT_CALLS) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }


    error = property_get_int<uint64_t>(RVS_CONF_MATRIX_SIZE_KEYA, &edp_matrix_size_a, EDP_DEFAULT_MATRIX_SIZE);
    if (error == 1) {
        msg = "invalid '" +
        std::string(RVS_CONF_MATRIX_SIZE_KEYA) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    error = property_get_int<uint64_t>(RVS_CONF_MATRIX_SIZE_KEYB, &edp_matrix_size_b, EDP_DEFAULT_MATRIX_SIZE);
    if (error == 1) {
        msg = "invalid '" +
        std::string(RVS_CONF_MATRIX_SIZE_KEYB) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    error = property_get_int<uint64_t>(RVS_CONF_MATRIX_SIZE_KEYC, &edp_matrix_size_c, EDP_DEFAULT_MATRIX_SIZE);
    if (error == 1) {
        msg = "invalid '" +
        std::string(RVS_CONF_MATRIX_SIZE_KEYC) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    error = property_get_int<int>(RVS_CONF_TRANS_A, &edp_trans_a, EDP_DEFAULT_TRANS_A);
    if (error == 1) {
        msg = "invalid '" +
        std::string(RVS_CONF_TRANS_A) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    error = property_get_int<int>(RVS_CONF_TRANS_B, &edp_trans_b, EDP_DEFAULT_TRANS_B);
    if (error == 1) {
        msg = "invalid '" +
        std::string(RVS_CONF_TRANS_B) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    error = property_get<float>(RVS_CONF_ALPHA_VAL, &edp_alpha_val, EDP_DEFAULT_ALPHA_VAL);
    if (error == 1) {
        msg = "invalid '" +
        std::string(RVS_CONF_ALPHA_VAL) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    error = property_get<float>(RVS_CONF_BETA_VAL, &edp_beta_val, EDP_DEFAULT_BETA_VAL);
    if (error == 1) {
        msg = "invalid '" +
        std::string(RVS_CONF_BETA_VAL) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    error = property_get_int<int>(RVS_CONF_LDA_OFFSET, &edp_lda_offset, EDP_DEFAULT_LDA_OFFSET);
    if (error == 1) {
        msg = "invalid '" +
        std::string(RVS_CONF_LDA_OFFSET) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    error = property_get_int<int>(RVS_CONF_LDB_OFFSET, &edp_ldb_offset, EDP_DEFAULT_LDB_OFFSET);
    if (error == 1) {
        msg = "invalid '" +
        std::string(RVS_CONF_LDB_OFFSET) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    error = property_get_int<int>(RVS_CONF_LDC_OFFSET, &edp_ldc_offset, EDP_DEFAULT_LDC_OFFSET);
    if (error == 1) {
        msg = "invalid '" +
        std::string(RVS_CONF_LDC_OFFSET) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    error = property_get_int<uint64_t>(RVS_CONF_ITERATIONS, &edp_wave_iterations, EDP_DEFAULT_WAVE_ITERATIONS);
    if (error == 1) {
        msg = "invalid '" +
        std::string(RVS_CONF_ITERATIONS) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    error = property_get_int<uint64_t>(RVS_CONF_HALT_WAVES, &edp_halt_timer, EDP_DEFAULT_HALT_WAVES);
    if (error == 1) {
        msg = "invalid '" +
        std::string(RVS_CONF_HALT_WAVES) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    error = property_get_int<uint64_t>(RVS_CONF_RESTART_WAVE_TIMER, &edp_restart_wave_timer, EDP_DEFAULT_RESTART_WAVE_TIMER);
    if (error == 1) {
        msg = "invalid '" +
        std::string(RVS_CONF_RESTART_WAVE_TIMER) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }
    error = property_get<bool>(RVS_CONF_BROADCAST_WAVE, &edp_broadast_wave, EDP_DEFAULT_BROADCAST_WAVE);
    if (error == 1) {
        msg = "invalid '" +
        std::string(RVS_CONF_BROADCAST_WAVE) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }




    return bsts;
}

/**
 * @brief reads all common configuration keys from
 * the module's properties collection
 * @return true if no fatal error occured, false otherwise
 */
bool edp_action::get_all_common_config_keys(void) {
    string msg, sdevid, sdev;
    int error;
    bool bsts = true;

    // get <device> property value (a list of gpu id)
    if (int sts = property_get_device()) {
      switch (sts) {
      case 1:
        msg = "Invalid 'device' key value.";
        break;
      case 2:
        msg = "Missing 'device' key.";
        break;
      }
      rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
      bsts = false;
    }

    // get the <deviceid> property value if provided
    if (property_get_int<uint16_t>(RVS_CONF_DEVICEID_KEY,
                                  &property_device_id, 0u)) {
      msg = "Invalid 'deviceid' key value.";
      rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
      bsts = false;
    }

    // get the other action/EDP related properties
    if (property_get(RVS_CONF_PARALLEL_KEY, &property_parallel, false)) {
      msg = "invalid '" +
          std::string(RVS_CONF_PARALLEL_KEY) + "' key value";
      rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
      bsts = false;
    }

    error = property_get_int<uint64_t>
    (RVS_CONF_COUNT_KEY, &property_count, DEFAULT_COUNT);
    if (error != 0) {
      msg = "invalid '" +
          std::string(RVS_CONF_COUNT_KEY) + "' key value";
      rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
      bsts = false;
    }

    error = property_get_int<uint64_t>
    (RVS_CONF_WAIT_KEY, &property_wait, DEFAULT_WAIT);
    if (error != 0) {
      msg = "invalid '" +
          std::string(RVS_CONF_WAIT_KEY) + "' key value";
      bsts = false;
    }

    error = property_get_int<uint64_t>
    (RVS_CONF_DURATION_KEY, &property_duration, RVS_DEFAULT_DURATION);
    if (error == 1) {
      msg = "invalid '" +
          std::string(RVS_CONF_DURATION_KEY) + "' key value";
      rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
      bsts = false;
    }

    return bsts;
}

/**
 * @brief gets the number of ROCm compatible AMD GPUs
 * @return run number of GPUs
 */
int edp_action::get_num_amd_gpu_devices(void) {
    int hip_num_gpu_devices;
    string msg;

    hipGetDeviceCount(&hip_num_gpu_devices);
    if (hip_num_gpu_devices == 0) {  // no AMD compatible GPU
        msg = action_name + " " + MODULE_NAME + " " + EDP_NO_COMPATIBLE_GPUS;
        rvs::lp::Log(msg, rvs::logerror);

        if (bjson) {
            unsigned int sec;
            unsigned int usec;
            rvs::lp::get_ticks(&sec, &usec);
            void *json_root_node = rvs::lp::LogRecordCreate(MODULE_NAME,
                            action_name.c_str(), rvs::loginfo, sec, usec);
            if (!json_root_node) {
                // log the error
                string msg = std::string(JSON_CREATE_NODE_ERROR);
                rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
                return -1;
            }

            rvs::lp::AddString(json_root_node, "ERROR", EDP_NO_COMPATIBLE_GPUS);
            rvs::lp::LogRecordFlush(json_root_node);
        }
        return 0;
    }
    return hip_num_gpu_devices;
}


/**
 * @brief gets all selected GPUs and starts the worker threads
 * @return run result
 */
int edp_action::get_all_selected_gpus(void) {
    int hip_num_gpu_devices;
    bool amd_gpus_found = false;
    map<int, uint16_t> edp_gpus_device_index;
    std::string msg;
    char buff[75];
    uint32_t iterations  = 0;

    hip_num_gpu_devices = get_num_amd_gpu_devices();
    if (hip_num_gpu_devices < 1)
        return hip_num_gpu_devices;

    //system("./rocm_edp_helper -l 1000000 &");
    //system(sprintf("./rocm_edp_helper -l %d &", edp_wave_iterations));
    sprintf(buff,  "./rocm_edp_helper -l %d &", edp_wave_iterations);
    system(buff);

    // iterate over all available & compatible AMD GPUs
    for (int i = 0; i < hip_num_gpu_devices; i++) {
        // get GPU device properties
        hipDeviceProp_t props;
        hipGetDeviceProperties(&props, i);

        // compute device location_id (needed in order to identify this device
        // in the gpus_id/gpus_device_id list
        unsigned int dev_location_id =
            ((((unsigned int) (props.pciBusID)) << 8) | (props.pciDeviceID));

        uint16_t devId;
        if (rvs::gpulist::location2device(dev_location_id, &devId)) {
          continue;
        }

        // filter by device id if needed
        if (property_device_id > 0 && property_device_id != devId)
          continue;

        // check if this GPU is part of the GPU stress test
        // (device = "all" or the gpu_id is in the device: <gpu id> list)
        bool cur_gpu_selected = false;
        uint16_t gpu_id;
        // if not and AMD GPU just continue
        if (rvs::gpulist::location2gpu(dev_location_id, &gpu_id))
          continue;


        if (property_device_all) {
            cur_gpu_selected = true;
        } else {
            // search for this gpu in the list
            // provided under the <device> property
            auto it_gpu_id = find(property_device.begin(),
                                  property_device.end(),
                                  gpu_id);

            if (it_gpu_id != property_device.end())
                cur_gpu_selected = true;
        }

        if (cur_gpu_selected) {
            edp_gpus_device_index.insert
                (std::pair<int, uint16_t>(i, gpu_id));
            amd_gpus_found = true;
        }
    }

    if (amd_gpus_found) {
        if (do_gpu_stress_test(edp_gpus_device_index))
            return 0;

        return -1;
    } else {
      msg = "No devices match criteria from the test configuration.";
      rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
      return -1;
    }

    return 0;
}

/**
 * @brief runs the whole EDP logic
 * @return run result
 */
int edp_action::run(void) {
    string msg;

    // get the action name
    if (property_get(RVS_CONF_NAME_KEY, &action_name)) {
      rvs::lp::Err("Action name missing", MODULE_NAME_CAPS);
      return -1;
    }

    // check for -j flag (json logging)
    if (property.find("cli.-j") != property.end())
        bjson = true;

    if (!get_all_common_config_keys())
        return -1;
    if (!get_all_edp_config_keys())
        return -1;

    if (property_duration > 0 && (property_duration < edp_ramp_interval)) {
        msg = "'" +
            std::string(RVS_CONF_DURATION_KEY) + "' cannot be less than '" +
            std::string(RVS_CONF_RAMP_INTERVAL_KEY) + "'";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        return -1;
    }

    return get_all_selected_gpus();
}

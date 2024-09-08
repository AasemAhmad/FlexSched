#include "ResourceManager/ResourceManager.hpp"
#include "External/Batsched/pempek_assert.hpp"
#include "ResourceManager/Algorithms/CPSolver/HPCWorkloadSolverCP.hpp"
#include "ResourceManager/Comm/Comm.hpp"
#include "Settings.hpp"
#include "Shared/Utils.hpp"
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <chrono>
#include <format>
#include <loguru.hpp>
#include <memory>
#include <sstream>
#include <thread>
#include <utility>

Resource::Resource(const std::string &r_id, const ResourceType &res_type, const int capacity) : id(r_id), type(res_type)
{
    PPK_ASSERT_ERROR(capacity >= 1, "Invalid resource capacity %d", capacity);

    this->resource_units = IntervalSet::ClosedInterval(0, capacity - 1);
    this->available_at.assign(capacity, 0);
}

void Resource::set_resource_units_available_at(const IntervalSet &res_units, double date)
{
    for (auto it = res_units.elements_begin(); it != res_units.elements_end(); ++it)
    {
        size_t resource_unit_index = *it;
        this->available_at[resource_unit_index] = date;
    }
}
void Resource::advance_free_resource_units_availability_to_current_date(double date)
{
    IntervalSet free_resource_units = get_free_resource_units_at_current_date(date);
    this->set_resource_units_available_at(free_resource_units, date);
}

IntervalSet Resource::get_free_resource_units_at_current_date(double date) const
{
    IntervalSet free_resource_units;
    for (size_t res_unit_index = 0; res_unit_index < available_at.size(); ++res_unit_index)
    {
        if (this->available_at[res_unit_index] <= date)
        {
            free_resource_units.insert(res_unit_index);
        }
    }
    return free_resource_units;
}

ResourceManager::ResourceManager(WorkloadManagerPtr workload_manager, SchedulingDecisionPtr decision)
    : ISchedulingAlgorithm(workload_manager, decision)
{}

ResourceManager::~ResourceManager() = default;

void ResourceManager::set_total_nb_compute_resources(int total_nb_compute_resources)
{
    PPK_ASSERT_ERROR(this->_total_nb_compute_resources == -1, "total_nb_compute_resources can only be set once");
    PPK_ASSERT_ERROR(total_nb_compute_resources > 0, "total_nb_compute_resources must be greater than 0");
    this->_total_nb_compute_resources = total_nb_compute_resources;
}

void ResourceManager::set_total_nb_io_resources(int total_nb_io_resources)
{
    PPK_ASSERT_ERROR(this->_total_nb_io_resources == -1, "total_nb_io_resources can only be set once");
    PPK_ASSERT_ERROR(total_nb_io_resources > 0, "total_nb_io_resources must be greater than 0");
    this->_total_nb_io_resources = total_nb_io_resources;
}

void ResourceManager::on_simulation_start(double date)
{
    LOG_F(INFO, "%s started", source_location_to_string(std::source_location::current()).c_str());

    this->_compute_resources = std::make_shared<Resource>("com_res", ResourceType::COMPUTE, this->_total_nb_compute_resources);
    this->_io_resources = std::make_shared<Resource>("io_res", ResourceType::IO, this->_total_nb_io_resources);

    for (const auto &workload : *this->_workload_manager->workloads_queue)
    {
        // FIXME check in case we have two workloads with same submission time
        double future_date = workload->_get_workload_submission_time();
        Comm::call_me_later(_decision, future_date, date);
    }
}

void ResourceManager::on_simulation_end(double date) { LOG_F(INFO, "Simulation is finished at time %g", date); }

void ResourceManager::on_requested_call(double date)
{
    LOG_F(INFO, "%s started", source_location_to_string(std::source_location::current()).c_str());

    for (const auto &workload : this->_workload_manager->get_released_workloads_at_current_date(date))
    {
        PPK_ASSERT_ERROR(workload->_submit_time <= date, "Workload %s should not be released now", workload->get_id().c_str());
        LOG_F(INFO, "workload %s is released", workload->get_id().c_str());

        for (auto job : *workload->_jobs)
        {
            LOG_F(INFO, "job %s released ", job->id.c_str());
            this->_workload_manager->update_job_after_release(job, date);
            this->_workload_manager->map_job_profiles_to_modes(job, this->_total_nb_io_resources);
            this->_jobs_released_recently.emplace_back(job->id);
        }
    }

    LOG_F(INFO, "%s finished", source_location_to_string(std::source_location::current()).c_str());
}

void ResourceManager::make_decisions(double date)
{
    LOG_F(INFO, "%s started", source_location_to_string(std::source_location::current()).c_str());

    this->_workload_manager->_date = date;

    this->get_released_jobs();

    if (_workload_manager->waiting_queue->nb_elements() > 0 && _workload_manager->_altered)
    {
        LOG_F(INFO, "current date = %g", date);

        _compute_resources->advance_free_resource_units_availability_to_current_date(date);
        _io_resources->advance_free_resource_units_availability_to_current_date(date);

        Solution solution;
        this->compute_schedule(date, solution);

        LOG_F(INFO, "solution: %s", solution.get_solution_as_string().c_str());

        map_solution_to_static_schedule(solution);
    }

    _workload_manager->execution_queue->sort_queue(Job::compare_by_start_time);

    std::ranges::for_each(*_workload_manager->execution_queue, [](const auto &item) {
        LOG_F(INFO, "Job %s, start time = %ld, nb_compute_res_units = %ld, nb_io_res_units = %ld, walltime = %g",
              item->id.c_str(), item->allocation.start_time, item->allocation.selected_mode.nb_requested_compute_resource_units,
              item->allocation.selected_mode.nb_requested_io_resource_units,
              static_cast<double>(item->allocation.selected_walltime));
    });

    execute_jobs_on_batsim(date);

    if (_workload_manager->execution_queue->is_empty() && _no_more_static_job_to_submit_received &&
        (this->_nb_jobs_submitted == this->_nb_jobs_completed))
    {
        LOG_F(INFO, "End of simulation");
        Comm::end_simulation(_decision, date);
    }
}

void ResourceManager::inverse_assigned_io_res_units(const JobPtr &job, double date)
{
    if (job->allocation.selected_mode.nb_requested_io_resource_units == 0)
    {
        // FIXME
        return;
    }

    if (job->info.current_activity_index == 0)
    {
        LOG_F(INFO, "inverse io res units for Job %s which has just started", job->id.c_str());

        IntervalSet free_io_res_units = _io_resources->get_free_resource_units_at_current_date(date);
        job->allocation.used_io_res_units = free_io_res_units.left(job->allocation.selected_mode.nb_requested_io_resource_units);
    } else
    {
        size_t nb_previous_io_res_units =
            job->allocation.io_res_units_allocation_per_activity.at(job->info.current_activity_index - 1).size();
        LOG_F(INFO, "job %s was previously assigned to %ld io res units ", job->id.c_str(), nb_previous_io_res_units);

        if (job->allocation.selected_mode.nb_requested_io_resource_units > nb_previous_io_res_units)
        {
            LOG_F(INFO, "Job %s is to be expanded to more io units", job->id.c_str());
            this->expand_job_by_given_nb_io_res_units(job, date);
        } else if (job->allocation.selected_mode.nb_requested_io_resource_units < nb_previous_io_res_units)
        {
            LOG_F(INFO, "job %s io units is to be shrunk", job->id.c_str());
            this->shrink_job_by_given_io_res_units(job, date);
        } else
        {
            LOG_F(INFO, "job %s will continue with the same nb of io res units", job->id.c_str());
            this->keep_current_io_units(job);
        }
    }

    LOG_F(INFO, "job %s is allocated to %ld io res units", job->id.c_str(), job->allocation.used_io_res_units.size());
    PPK_ASSERT_ERROR(job->allocation.used_io_res_units.size() == job->allocation.selected_mode.nb_requested_io_resource_units,
                     "Invalid assignment for number of io res units");
}

void ResourceManager::inverse_assigned_compute_res_units(const JobPtr &job, double date) {}

void ResourceManager::map_solution_to_static_schedule(const Solution &solution)
{
    // FIXME
    LOG_F(INFO, "%s started", source_location_to_string(std::source_location::current()).c_str());

    for (const auto &job : *_workload_manager->execution_queue)
    {
        auto solution_it = solution.solution_info.find(job->id);
        PPK_ASSERT_ERROR(solution_it != solution.solution_info.cend(), "Invalid Job ID %s", job->id.c_str());
        auto start_time = static_cast<double>(solution_it->second.start_time);

        size_t nb_compute_machines = solution_it->second.nb_compute_res_units;
        size_t nb_io_units = solution_it->second.nb_io_res_units;
        PPK_ASSERT_ERROR(nb_io_units >= 0, "Invalid nb io res units");

        if (job->info.job_type == JobType::DYNAMIC_IO_CHECKPOINT || job->info.job_type == JobType::DYNAMIC_IO_STAGE_IN)
        {
            LOG_F(INFO, "mapping dynamic io job %s", job->id.c_str());
            Mode mode;
            mode.nb_requested_compute_resource_units = nb_compute_machines;
            mode.nb_requested_io_resource_units = nb_io_units;

            job->allocation.selected_mode = mode;
            job->allocation.selected_walltime = job->info.walltime;
        } else
        {
            LOG_F(INFO, "mapping static job %s", job->id.c_str());

            job->allocation.selected_mode = job->get_mode(nb_compute_machines, nb_io_units);
            job->allocation.selected_walltime = job->info.walltime;
            job->allocation.start_time = start_time;
        }
    }
}

void ResourceManager::expand_job_by_given_nb_compute_res_units(const JobPtr &job, double date)
{
    PPK_ASSERT_ERROR(job->info.current_activity_index > 0, "Invalid job index %ld", job->info.current_activity_index);

    IntervalSet previous_machines = job->allocation.used_compute_res_units;
    PPK_ASSERT_ERROR(previous_machines.size() < job->allocation.selected_mode.nb_requested_compute_resource_units,
                     "Invalid call for expand compute op");

    IntervalSet free_machines = _compute_resources->get_free_resource_units_at_current_date(date);

    free_machines -= previous_machines;

    PPK_ASSERT_ERROR(free_machines.size() + previous_machines.size() >=
                     (job->allocation.selected_mode.nb_requested_compute_resource_units));

    job->allocation.used_compute_res_units = previous_machines;
    job->allocation.used_compute_res_units.insert(
        free_machines.left(job->allocation.selected_mode.nb_requested_compute_resource_units - previous_machines.size()));

    LOG_F(INFO, "job %s used compute res units = %s", job->id.c_str(),
          job->allocation.used_compute_res_units.to_string_hyphen().c_str());
}

void ResourceManager::shrink_job_by_given_nb_compute_res_units(const JobPtr &job, double date)
{
    PPK_ASSERT_ERROR(job->info.current_activity_index > 0 && job->allocation.selected_mode.nb_requested_compute_resource_units <
                                                                 job->allocation.used_compute_res_units.size(),
                     "Invalid call for shrink compute op");
    IntervalSet previous_compute_res_units = job->allocation.used_compute_res_units;
    IntervalSet current_compute_res_units =
        previous_compute_res_units.left(job->allocation.selected_mode.nb_requested_compute_resource_units);
    IntervalSet freed_machines = previous_compute_res_units - current_compute_res_units;
    _compute_resources->set_resource_units_available_at(freed_machines, date);
    job->allocation.used_compute_res_units = current_compute_res_units;
}

void ResourceManager::keep_current_compute_res_units(const JobPtr &job)
{
    PPK_ASSERT_ERROR(job->info.current_activity_index > 0 && job->allocation.selected_mode.nb_requested_compute_resource_units ==
                                                                 job->allocation.used_compute_res_units.size(),
                     "Invalid call for keep current io op");
}

void ResourceManager::expand_job_by_given_nb_io_res_units(const JobPtr &job, double date)
{
    PPK_ASSERT_ERROR(job->info.current_activity_index > 0 &&
                         job->allocation.selected_mode.nb_requested_io_resource_units > job->allocation.used_io_res_units.size(),
                     "Invalid call for expand io op");

    IntervalSet previous_io_units = job->allocation.used_io_res_units;
    IntervalSet free_io_units = _io_resources->get_free_resource_units_at_current_date(date);

    free_io_units -= previous_io_units;

    PPK_ASSERT_ERROR((free_io_units.size() + previous_io_units.size()) >=
                         job->allocation.selected_mode.nb_requested_io_resource_units,
                     "not sufficient free io res units");

    job->allocation.used_io_res_units = previous_io_units;
    job->allocation.used_io_res_units.insert(
        free_io_units.left(job->allocation.selected_mode.nb_requested_io_resource_units - previous_io_units.size()));

    LOG_F(INFO, "job %s used io res units = %s", job->id.c_str(), job->allocation.used_io_res_units.to_string_hyphen().c_str());
}

void ResourceManager::shrink_job_by_given_io_res_units(const JobPtr &job, double date)
{
    PPK_ASSERT_ERROR(job->info.current_activity_index > 0 &&
                         job->allocation.selected_mode.nb_requested_io_resource_units < job->allocation.used_io_res_units.size(),
                     "Invalid call for shrink io op");

    IntervalSet previous_io_units = job->allocation.used_io_res_units;
    IntervalSet current_io_units = previous_io_units.left(job->allocation.selected_mode.nb_requested_io_resource_units);
    _io_resources->set_resource_units_available_at(previous_io_units - current_io_units, date);
    job->allocation.used_io_res_units = current_io_units;
}

void ResourceManager::keep_current_io_units(const JobPtr &job)
{
    PPK_ASSERT_ERROR(job->info.current_activity_index > 0 &&
                         job->allocation.selected_mode.nb_requested_io_resource_units == job->allocation.used_io_res_units.size(),
                     "Invalid call for keep current io op");
}

void ResourceManager::on_job_release(double date, const std::vector<std::string> &job_ids)
{
    // TODO if needed
}

void ResourceManager::on_job_end(double date, const std::vector<std::string> &job_ids)
{
    LOG_F(INFO, "%s started", source_location_to_string(std::source_location::current()).c_str());

    _workload_manager->_date = date;
    _workload_manager->_altered = true;
    _jobs_ended_recently.insert(_jobs_ended_recently.end(), job_ids.begin(), job_ids.end());

    for (const std::string &ended_job_id : _jobs_ended_recently)
    {
        JobPtr ended_job = _workload_manager->get_job(ended_job_id);
        PPK_ASSERT_ERROR(ended_job, "Job %s could not be found", ended_job_id.c_str());
        ended_job->info.completion_time = date;
        handle_activity_complete(ended_job, date);
    }

    LOG_F(INFO, "End of job complete function");
    ++this->_nb_jobs_completed;
    _jobs_ended_recently.clear();
}

void ResourceManager::handle_activity_complete(const JobPtr &job, double date)
{
    const std::string activity_id = job->id;
    if (activity_id.find("checkpoint") != std::string::npos)
    {
        LOG_F(INFO, "handling checkpoint complete event");
        this->handle_checkpoint_complete(activity_id, date);
    } else if (activity_id.find("stage_in") != std::string::npos)
    {
        LOG_F(INFO, "handling stage_in complete event");
        this->handle_stage_in_complete(activity_id, date);
    } else
    {
        if (job->info.current_activity_index < job->info.nb_activities - 1)
        {
            this->post_process_current_activity(date, activity_id);
        } else
        {
            LOG_F(INFO, "Job nb_activities = %ld", job->info.nb_activities);
            this->finalize_job(activity_id, date);
        }
    }
}

void ResourceManager::on_job_killed(double date, const std::vector<std::string> &job_ids)
{
    // TODO
}
void ResourceManager::on_machine_state_changed(double date, IntervalSet machines, int new_state)
{
    // TODO
}
void ResourceManager::on_no_more_external_event_to_occur(double date)
{
    // TODO
}
void ResourceManager::on_answer_energy_consumption(double date, double consumed_joules)
{
    // TODO
}
void ResourceManager::on_machine_available_notify_event(double date, IntervalSet machines)
{
    // TODO
}
void ResourceManager::on_machine_unavailable_notify_event(double date, IntervalSet machines)
{
    // TODO
}
void ResourceManager::on_query_estimate_waiting_time(double date, const std::string &job_id)
{
    // TODO
}

void ResourceManager::post_process_current_activity(double date, const std::string &job_id)
{
    LOG_F(INFO, "%s started", source_location_to_string(std::source_location::current()).c_str());

    JobPtr job = _workload_manager->get_job(job_id);
    PPK_ASSERT_ERROR(job, "job %s was not found", job_id.c_str());
    PPK_ASSERT_ERROR(job->info.current_activity_index < job->info.nb_activities - 1, "Invalid call for %s",
                     source_location_to_string(std::source_location::current()).c_str());

    ++job->info.current_activity_index;
    _workload_manager->_date = date;
    _workload_manager->_altered = true;

    LOG_F(INFO,
          "job %s ->  activity completed event received at %g activity_index = %ld was allocated to-> compute res: %s, io "
          "resources:%s",
          job->id.c_str(), date, job->info.current_activity_index,
          job->allocation.used_compute_res_units.to_string_hyphen().c_str(),
          job->allocation.used_io_res_units.to_string_hyphen().c_str());

    if (this->_workload_manager->is_checkpointing_to_be_requested(job))
    {
        job->info.is_in_checkpointing_op = true;
        if (Settings::BLOCKED_CR_OBLIVIOUS)
        {
            this->create_and_submit_dynamic_checkpoint_job(job, date);
        } else
        {
            if (auto [is_pending, checkpoint_activity_id] = _workload_manager->is_checkpointing_op_pending(job->id); is_pending)
            {
                _workload_manager->remove_job_form_execution_queue(checkpoint_activity_id);
                _workload_manager->remove_job_from_waiting_queue(checkpoint_activity_id);
            }
            this->insert_checkpointing_request_into_job_queues(job, date);
        }
    }

    if (_workload_manager->should_advance_to_next_activity(job))
    {
        if (_workload_manager->should_resume_execution_with_same_configuration(job))
        {
            this->resume_job_execution_with_same_configuration(job, date);
        } else
        {
            job->allocation.start_time = date;
            this->_workload_manager->execution_queue->append_element(job);
            this->_workload_manager->waiting_queue->append_element(job);
            this->_workload_manager->_altered = true;
            LOG_F(INFO, "Job job_id = %s, is inserted into the queues", job->id.c_str());
            _compute_resources->set_resource_units_available_at(job->allocation.used_compute_res_units, date);
            _io_resources->set_resource_units_available_at(job->allocation.used_io_res_units, date);
        }
    }
    _workload_manager->_date = date;
    job->info.has_scheduled = true;
}

void ResourceManager::insert_checkpointing_request_into_job_queues(const JobPtr &parent_job, double date)
{
    auto job_factory = std::make_shared<DynamicJobFactory>();

    auto profile = std::make_shared<Profile>();
    profile->type = ProfileType::PARALLEL_HOMOGENEOUS_PFS;
    auto cr_job = job_factory->create_dynamic_io_job(parent_job, JobType::DYNAMIC_IO_CHECKPOINT, profile, date);
    // FIXME
    _io_resources->set_resource_units_available_at(parent_job->allocation.used_io_res_units, date);
    this->_workload_manager->execution_queue->append_element(cr_job);
    this->_workload_manager->waiting_queue->append_element(cr_job);
    this->_workload_manager->_altered = true;
    _workload_manager->_date = date;
}

void ResourceManager::handle_checkpoint_complete(const std::string &ended_job_id, double date)
{
    std::string parent_id = _workload_manager->get_parent_job_id(ended_job_id);
    JobPtr parent_job = _workload_manager->get_job(parent_id);
    PPK_ASSERT_ERROR(parent_job != nullptr, "Parent job was not found with this id %s", parent_id.c_str());
    parent_job->info.is_in_checkpointing_op = false;
    bool should_resume_job = (parent_job->info.current_activity_index < parent_job->info.nb_activities) &&
                             (!Settings::ASYNCHRONOUS_CHECKPOINTING_ENABLED);
    if (should_resume_job)
    {
        this->resume_job_execution_with_same_configuration(parent_job, date);
    }
}

void ResourceManager::handle_stage_in_complete(const std::string &ended_job_id, double date)
{
    std::string parent_id = _workload_manager->get_parent_job_id(ended_job_id);
    JobPtr parent_job = _workload_manager->get_job(parent_id);
    PPK_ASSERT_ERROR(parent_job != nullptr, "Parent job was not found with this id %s", parent_id.c_str());
    this->resume_job_execution_after_stage_in(parent_job, date);
}

void ResourceManager::finalize_job(const std::string &ended_job_id, double date)
{
    LOG_F(INFO, "%s started", source_location_to_string(std::source_location::current()).c_str());
    LOG_F(INFO, "Job %s is completed", ended_job_id.c_str());

    JobPtr ended_job = _workload_manager->get_job(ended_job_id);
    PPK_ASSERT(ended_job, "ended job %s could not be found", ended_job_id.c_str());
    IntervalSet last_activity_compute_res_units =
        ended_job->allocation.compute_res_units_allocation_per_activity.at(ended_job->info.current_activity_index);
    IntervalSet last_activity_io_res_units =
        ended_job->allocation.io_res_units_allocation_per_activity.at(ended_job->info.current_activity_index);
    LOG_F(INFO, "Job %s is complete, last_activity_compute_res_units %s, last_activity_io_res_units %s ", ended_job->id.c_str(),
          last_activity_compute_res_units.to_string_hyphen().c_str(), last_activity_io_res_units.to_string_hyphen().c_str());
    _compute_resources->set_resource_units_available_at(last_activity_compute_res_units, date);
    _io_resources->set_resource_units_available_at(last_activity_io_res_units, date);
}

void ResourceManager::resume_job_execution_after_stage_in(const JobPtr &job, double date)
{
    auto [allocation_it, emplaced_nodes] = job->allocation.compute_res_units_allocation_per_activity.try_emplace(
        job->info.current_activity_index, job->allocation.used_compute_res_units);
    PPK_ASSERT_ERROR(emplaced_nodes, "Failed, key already exists");

    auto [io_units_it, emplaced_io_units] = job->allocation.io_res_units_allocation_per_activity.try_emplace(
        job->info.current_activity_index, job->allocation.used_io_res_units);
    PPK_ASSERT_ERROR(emplaced_io_units, "Failed, Key already exists");

    ProfilePtr profile_info = _workload_manager->create_dynamic_addition_io_profile(job, date);

    size_t nb_nodes = job->allocation.selected_mode.nb_requested_compute_resource_units;

    PPK_ASSERT_ERROR(profile_info, "Failed to create profile");

    job->allocation.start_time = std::floor(date);

    int pfs_id = this->_total_nb_compute_resources;

    IoData io_data;
    io_data.io_machines = job->allocation.used_compute_res_units;
    io_data.io_machines.insert(pfs_id);

    const std::string workload_id = Job::get_workload_id(job->id);
    auto workload = _workload_manager->workloads_queue->get_element(workload_id);
    PPK_ASSERT_ERROR(workload, "workload %s could not be found", workload_id.c_str());

    LOG_F(INFO, "workload id = %s", workload->get_id().c_str());

    Comm::command_static_job_execution(job, _decision, workload, profile_info, io_data, date);

    double reservation_time =
        date + (job->info.nb_activities - job->info.current_activity_index) * (double)job->allocation.selected_walltime;

    _compute_resources->set_resource_units_available_at(
        job->allocation.compute_res_units_allocation_per_activity.at(job->info.current_activity_index), reservation_time);

    _io_resources->set_resource_units_available_at(
        job->allocation.io_res_units_allocation_per_activity.at(job->info.current_activity_index), reservation_time);
}

void ResourceManager::resume_job_execution_with_same_configuration(const JobPtr &job, double date)
{
    LOG_F(INFO, "%s started", source_location_to_string(std::source_location::current()).c_str());

    job->allocation.start_time = std::floor(date);
    IntervalSet not_allowed_machines;
    IntervalSet add_io_machines = job->allocation.used_compute_res_units;

    ProfilePtr profile_info = _workload_manager->create_compute_io_profile(job, date);

    IoData io_data;
    io_data.pfs_id = this->_total_nb_compute_resources;
    io_data.storage_mapping = {{"pfs", this->_total_nb_compute_resources}};
    std::string workload_name = Job::get_workload_id(job->id);

    const std::string workload_id;
    auto workload = _workload_manager->workloads_queue->get_element(workload_id);
    PPK_ASSERT_ERROR(workload, "workload %s could not be found", workload_id.c_str());

    Comm::command_static_job_execution(job, _decision, workload, profile_info, io_data, date);

    auto [activity_compute_allocation_it, emplaced_nodes] = job->allocation.compute_res_units_allocation_per_activity.try_emplace(
        job->info.current_activity_index, job->allocation.used_compute_res_units);
    PPK_ASSERT_ERROR(emplaced_nodes, "Failed, key already exists");

    auto [activity_io_allocation_it, emplaced_io_units] = job->allocation.io_res_units_allocation_per_activity.try_emplace(
        job->info.current_activity_index,
        job->allocation.io_res_units_allocation_per_activity.at(job->info.current_activity_index - 1));
    PPK_ASSERT_ERROR(emplaced_io_units, "Failed, Key already exists");

    double reservation_time =
        date + (job->info.nb_activities - job->info.current_activity_index) * (double)job->allocation.selected_walltime;

    _compute_resources->set_resource_units_available_at(
        job->allocation.compute_res_units_allocation_per_activity.at(job->info.current_activity_index), reservation_time);

    _io_resources->set_resource_units_available_at(
        job->allocation.io_res_units_allocation_per_activity.at(job->info.current_activity_index), reservation_time);
}

void ResourceManager::on_no_more_static_job_to_submit_received(double date)
{
    (void)date;

    LOG_F(INFO, "%s started", source_location_to_string(std::source_location::current()).c_str());

    _no_more_static_job_to_submit_received = true;
}

void ResourceManager::get_released_jobs()
{
    LOG_F(INFO, "%s started", source_location_to_string(std::source_location::current()).c_str());

    for (const std::string &new_job_id : _jobs_released_recently)
    {
        LOG_F(INFO, "Job %s released", new_job_id.c_str());

        const std::string workload_id = Job::get_workload_id(new_job_id);
        auto workload = _workload_manager->workloads_queue->get_element(workload_id);
        PPK_ASSERT_ERROR(workload, "workload %s was not found", workload_id.c_str());
        LOG_F(INFO, "nb_machines = %ld nb_io_res_units = %ld", this->_total_nb_compute_resources, this->_total_nb_io_resources);
        JobPtr new_job = workload->_get_job(new_job_id);
        PPK_ASSERT_ERROR(new_job, "workload %s has no job %s", workload_id.c_str(), new_job_id.c_str());
        _workload_manager->execution_queue->append_element(new_job);
        _workload_manager->waiting_queue->append_element(new_job);
        _workload_manager->_altered = true;
        ++this->_nb_jobs_submitted;
    }
}

void ResourceManager::compute_schedule(double date, Solution &solution)
{
    LOG_F(INFO, "%s started", source_location_to_string(std::source_location::current()).c_str());
    _workload_manager->_date = date;

    PPK_ASSERT_ERROR(Settings::SolverSettings::USE_CP, "Only CP Solver is supported for now");

    std::vector<std::shared_ptr<Resource>> res_vec = {_compute_resources, _io_resources};

    HPCWorkloadSolverCP cpSolver(_workload_manager, res_vec);
    solution = cpSolver.solve();

    LOG_F(INFO, "%s", solution.get_solution_as_string().c_str());

    _workload_manager->_altered = false;
}

void ResourceManager::execute_jobs_on_batsim(double date)
{
    LOG_F(INFO, "%s started", source_location_to_string(std::source_location::current()).c_str());

    for (auto job_it = _workload_manager->execution_queue->begin(); job_it != _workload_manager->execution_queue->end();)
    {
        JobPtr job = (*job_it);
        job->id = _workload_manager->get_activity_id(job);

        LOG_F(INFO, "job %s reconfigurable = %s", job->id.c_str(), (job->config.reconfigurable) ? "true" : "false");

        if (job->allocation.start_time > date)
        {
            break;
        }

        if (job->info.job_type == JobType::DYNAMIC_IO_CHECKPOINT)
        {
            execute_dynamic_checkpoint_job(job, date);
        } else
        {
            if (job->info.current_activity_index == 0)
            {
                this->inverse_assigned_io_res_units(job, date);
                PPK_ASSERT_ERROR(job->allocation.used_io_res_units.size() >= 0);
                PPK_ASSERT_ERROR((double)job->allocation.selected_walltime > 0);
                this->execute_stage_in(job, date);

            } else
            {
                handle_job_subsequent_activities(job, date);
            }
        }
        job_it = _workload_manager->remove_job_form_execution_queue(job->id);
        _workload_manager->remove_job_from_waiting_queue(job->id);
    }
}

void ResourceManager::handle_job_subsequent_activities(const JobPtr &job, double date)
{
    LOG_F(INFO, "%s started", source_location_to_string(std::source_location::current()).c_str());
    PPK_ASSERT_ERROR(job->info.current_activity_index > 0 && job->info.current_activity_index < job->info.nb_activities);

    IntervalSet previous_allocated_compute_res_units =
        job->allocation.compute_res_units_allocation_per_activity.at(job->info.current_activity_index - 1);

    if (is_expand_op(job))
    {
        expand_job_by_given_nb_compute_res_units(job, date);
    } else if (is_shrink_op(job))
    {
        shrink_job_by_given_nb_compute_res_units(job, date);
    } else
    {
        keep_current_compute_res_units(job);
    }

    inverse_assigned_io_res_units(job, date);

    job->allocation.compute_res_units_allocation_per_activity[job->info.current_activity_index] =
        job->allocation.used_compute_res_units;
    job->allocation.io_res_units_allocation_per_activity[job->info.current_activity_index] = job->allocation.used_io_res_units;

    if (job->info.job_type == JobType::CONFIGURABLE)
    {
        check_job_new_reconfiguration(job);
    }

    LOG_F(INFO, "%s", source_location_to_string(std::source_location::current()).c_str());

    IntervalSet add_io_machines = job->allocation.used_compute_res_units;

    ProfilePtr profile_info = _workload_manager->create_dynamic_addition_io_profile(job, date);
    PPK_ASSERT_ERROR(profile_info, "Could not create profile for job %s", job->id.c_str());

    int pfs_id = this->_total_nb_compute_resources;
    IoData io_data;
    io_data.io_machines = job->allocation.used_compute_res_units;
    io_data.io_machines.insert(pfs_id);

    const std::string workload_id = Job::get_workload_id(job->id);
    auto workload = _workload_manager->workloads_queue->get_element(workload_id);
    PPK_ASSERT_ERROR(workload, "workload %s could not be found", workload_id.c_str());

    // Comm::register_additional_io_profile(job, decision, workload_name, profile, date);
    Comm::command_static_job_execution(job, _decision, workload, profile_info, io_data, date);

    double expected_finish_time =
        (job->info.nb_activities - job->info.current_activity_index) * static_cast<double>(job->allocation.selected_walltime);
    job->info.has_started = true;
    job->allocation.start_time = std::floor(date);

    _compute_resources->set_resource_units_available_at(job->allocation.used_compute_res_units, date + expected_finish_time);
    _io_resources->set_resource_units_available_at(job->allocation.used_io_res_units, date + expected_finish_time);
}

bool ResourceManager::is_shrink_op(const JobPtr &job) const
{
    PPK_ASSERT_ERROR(job->info.current_activity_index > 0, "Invalid call for %s",
                     source_location_to_string(std::source_location::current()).c_str());
    return (job->allocation.selected_mode.nb_requested_compute_resource_units <
            (int)job->allocation.compute_res_units_allocation_per_activity.at(job->info.current_activity_index - 1).size());
}

bool ResourceManager::is_expand_op(const JobPtr &job) const
{
    PPK_ASSERT_ERROR(job->info.current_activity_index > 0, "Invalid call for %s",
                     source_location_to_string(std::source_location::current()).c_str());
    return (job->allocation.selected_mode.nb_requested_compute_resource_units >
            (int)job->allocation.compute_res_units_allocation_per_activity.at(job->info.current_activity_index - 1).size());
}

bool ResourceManager::is_same_config(const JobPtr &job) const
{
    PPK_ASSERT_ERROR(job->info.current_activity_index > 0, "Invalid call for %s",
                     source_location_to_string(std::source_location::current()).c_str());
    return (job->allocation.selected_mode.nb_requested_compute_resource_units ==
            (int)job->allocation.compute_res_units_allocation_per_activity.at(job->info.current_activity_index - 1).size());
}

void ResourceManager::get_not_allowed_machines_when_expanding(const JobPtr &expanded_job, IntervalSet &not_allowed_machines)
{
    std::ranges::for_each(*_workload_manager->execution_queue, [&not_allowed_machines, &expanded_job, this](const auto &job) {
        using enum JobType;
        bool should_forbid_those_nodes =
            (job->id != expanded_job->id) &&
            (job->info.job_type == CONFIGURABLE && job->info.current_activity_index > 0 && this->is_expand_op(job));
        if (should_forbid_those_nodes)
        {
            not_allowed_machines.insert(
                job->allocation.compute_res_units_allocation_per_activity.at(job->info.current_activity_index - 1));
        }
    });
}

void ResourceManager::create_and_submit_dynamic_checkpoint_job(const JobPtr &parent_job, double date)
{
    LOG_F(INFO, "%s started", source_location_to_string(std::source_location::current()).c_str());

    ProfilePtr profile_info = _workload_manager->create_dynamic_io_checkpoint_profile(parent_job, date);
    JobPtr cr_job = _workload_manager->create_dynamic_io_checkpoint_job(parent_job, profile_info, date);

    PPK_ASSERT_ERROR(profile_info && cr_job, "Checkpointing job could not be created");

    double reservation_time = date + static_cast<double>(cr_job->allocation.selected_walltime) +
                              _workload_manager->get_job_estimated_walltime(parent_job);

    _compute_resources->set_resource_units_available_at(parent_job->allocation.used_compute_res_units, reservation_time);
    _io_resources->set_resource_units_available_at(parent_job->allocation.used_io_res_units, reservation_time);

    IoData io_data;
    io_data.pfs_id = this->_total_nb_compute_resources;
    io_data.storage_mapping = {{"pfs", this->_total_nb_compute_resources}};

    const std::string workload_id = Job::get_workload_id(parent_job->id);
    auto workload = _workload_manager->workloads_queue->get_element(workload_id);
    PPK_ASSERT_ERROR(workload, "workload %s was not found", workload_id.c_str());

    Comm::command_dynamic_job_execution(cr_job, _decision, workload, profile_info, io_data, date);
    this->_nb_jobs_submitted++;
}

void ResourceManager::execute_dynamic_checkpoint_job(const JobPtr &cr_job, double date)
{
    std::string parent_id = _workload_manager->get_parent_job_id(cr_job->id);
    LOG_F(INFO, "Job %s , its parent is job %s", cr_job->id.c_str(), parent_id.data());
    JobPtr parent_job = _workload_manager->get_job(parent_id);
    PPK_ASSERT_ERROR(parent_job != nullptr, "parent job %s was not found", parent_id.c_str());
    parent_job->info.is_in_checkpointing_op = true;

    ProfilePtr profile_info = _workload_manager->create_dynamic_io_checkpoint_profile(parent_job, date);

    double estimated_walltime = _workload_manager->get_activity_estimated_walltime(cr_job);

    double reservation_time = date + estimated_walltime + _workload_manager->get_job_estimated_walltime(parent_job);

    _compute_resources->set_resource_units_available_at(parent_job->allocation.used_compute_res_units, reservation_time);
    _io_resources->set_resource_units_available_at(parent_job->allocation.used_io_res_units, reservation_time);

    IoData io_data;
    io_data.pfs_id = this->_total_nb_compute_resources;
    io_data.storage_mapping = {{"pfs", this->_total_nb_compute_resources}};

    const std::string workload_id = Job::get_workload_id(cr_job->id);
    auto workload = _workload_manager->workloads_queue->get_element(workload_id);
    PPK_ASSERT_ERROR(workload, "workload %s was not found", workload_id.c_str());

    Comm::command_dynamic_job_execution(cr_job, _decision, workload, profile_info, io_data, date);
    this->_nb_jobs_submitted++;
}

void ResourceManager::execute_stage_in(const JobPtr &parent_job, double date)
{
    LOG_F(INFO, "%s", source_location_to_string(std::source_location::current()).c_str());

    IntervalSet free_machines = _compute_resources->get_free_resource_units_at_current_date(date);

    LOG_F(INFO, "date %g, free machines = %s, req_nb_res = %ld", date, free_machines.to_string_hyphen().c_str(),
          parent_job->allocation.selected_mode.nb_requested_compute_resource_units);
    PPK_ASSERT_ERROR(free_machines.size() >= parent_job->allocation.selected_mode.nb_requested_compute_resource_units);

    parent_job->allocation.used_compute_res_units =
        free_machines.left(parent_job->allocation.selected_mode.nb_requested_compute_resource_units);

    LOG_F(INFO, "parent_job allocated machines = %s", parent_job->allocation.used_compute_res_units.to_string_hyphen().c_str());

    ProfilePtr profile_info = _workload_manager->create_dynamic_io_stage_in_profile(parent_job, date);
    JobPtr stage_in_job = _workload_manager->create_dynamic_io_stage_in_job(parent_job, profile_info, date);

    const std::string workload_id = Job::get_workload_id(parent_job->id);
    auto workload = _workload_manager->workloads_queue->get_element(workload_id);
    PPK_ASSERT_ERROR(workload, "Workload %s cannot be found", workload_id.c_str());
    workload->_jobs->append_element(stage_in_job);

    double reservation_time =
        date + stage_in_job->allocation.selected_walltime + _workload_manager->get_job_estimated_walltime(parent_job);

    _compute_resources->set_resource_units_available_at(parent_job->allocation.used_compute_res_units, reservation_time);
    _io_resources->set_resource_units_available_at(parent_job->allocation.used_io_res_units, reservation_time);

    IoData io_data;
    io_data.pfs_id = this->_total_nb_compute_resources;
    io_data.storage_mapping = {{"pfs", this->_total_nb_compute_resources}};

    Comm::command_dynamic_job_execution(stage_in_job, _decision, workload, profile_info, io_data, date);
    this->_nb_jobs_submitted++;
}

void ResourceManager::create_and_submit_dynamic_delay(const JobPtr &parent_job, double date)
{
    // TODO
}
void ResourceManager::execute_delay(const JobPtr &job, double date)
{
    // TODO
}
void ResourceManager::create_and_submit_dynamic_parallel_task(const JobPtr &parent_job, double date)
{
    // TODO
}
void ResourceManager::execute_dynamic_parallel_task(const JobPtr &job, double date)
{
    // TODO
}

bool ResourceManager::check_job_new_reconfiguration(const JobPtr &job)
{
    LOG_F(INFO, "%s started", source_location_to_string(std::source_location::current()).c_str());

    PPK_ASSERT_ERROR(job->info.current_activity_index > 0 && job->info.current_activity_index < job->info.nb_activities);
    IntervalSet current_machines = job->allocation.used_compute_res_units;

    if (IntervalSet previous_machines =
            job->allocation.compute_res_units_allocation_per_activity.at(job->info.current_activity_index - 1);
        current_machines.size() > previous_machines.size())
    {
        PPK_ASSERT_ERROR(job->config.configurable && job->config.reconfigurable, "Job %s is not reconfigurable", job->id.c_str());
        PPK_ASSERT_ERROR((current_machines & previous_machines) == previous_machines);
    } else if (current_machines.size() < previous_machines.size())
    {
        PPK_ASSERT_ERROR(job->config.configurable && job->config.reconfigurable, "Job %s is not reconfigurable", job->id.c_str());
        PPK_ASSERT_ERROR((previous_machines & current_machines) == current_machines);
    } else
    {
        PPK_ASSERT_ERROR(previous_machines == current_machines);
    }
    return true;
}
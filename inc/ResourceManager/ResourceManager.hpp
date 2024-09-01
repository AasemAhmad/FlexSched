#pragma once

#include "External/Batsched/isalgorithm.hpp"
#include "Solution/Solution.hpp"
#include "WorkloadManager/WorkloadManager.hpp"
#include "WorkloadManager/workload.hpp"
#include <list>

enum class ResourceType
{
    COMPUTE = 0,
    IO = 1
};

class ResourceManager : public ISchedulingAlgorithm
{
  public:
    ResourceManager(WorkloadManagerPtr workload_manager, SchedulingDecisionPtr decision);

    ~ResourceManager() override;

  private:
    void on_simulation_start(double date) override;
    void on_simulation_end(double date) override;
    void on_activity_completed(double date, const std::string &job_id) override;
    void on_job_end(double date, const std::vector<std::string> &job_ids) override;
    void on_no_more_static_job_to_submit_received(double date) override;
    void on_job_release(double date, const std::vector<std::string> &job_ids) override;
    void on_requested_call(double date) override;
    void on_job_killed(double date, const std::vector<std::string> &job_ids) override;
    void on_machine_state_changed(double date, IntervalSet machines, int new_state) override;
    void on_no_more_external_event_to_occur(double date) override;
    void on_answer_energy_consumption(double date, double consumed_joules) override;
    void on_machine_available_notify_event(double date, IntervalSet machines) override;
    void on_machine_unavailable_notify_event(double date, IntervalSet machines) override;
    void on_query_estimate_waiting_time(double date, const std::string &job_id) override;
    void make_decisions(double date) override;

    void set_total_nb_compute_resources(int total_nb_compute_resources) override;
    void set_total_nb_io_resources(int total_nb_io_resources) override;

    void get_released_jobs();
    void insert_checkpointing_request_into_job_queues(const JobPtr &job, double date);
    void handle_checkpoint_complete(const std::string &ended_job_id, double date);
    void handle_stage_in_complete(const std::string &ended_job_id, double date);
    void handle_configurable_job_activity_complete(const JobPtr &job, double date);
    void handle_job_subsequent_activities(const JobPtr &job, double date);
    void handle_job_complete(const std::string &ended_job_id, double date);

    void create_and_submit_dynamic_checkpoint_job(const JobPtr &parent_job, double date);
    void execute_dynamic_checkpoint_job(const JobPtr &job, double date);
    void execute_stage_in(const JobPtr &parent_job, double date);

    void create_and_submit_dynamic_delay(const JobPtr &parent_job, double date);
    void execute_delay(const JobPtr &job, double date);
    void create_and_submit_dynamic_parallel_task(const JobPtr &parent_job, double date);
    void execute_dynamic_parallel_task(const JobPtr &job, double date);

    std::string get_parent_job_id(const std::string &io_job_id) const;
    std::pair<bool, std::string> is_checkpointing_op_pending(const std::string &parent_id) const;
    void compute_schedule(double date, Solution &sol);

    bool is_shrink_op(const JobPtr &job) const;
    bool is_expand_op(const JobPtr &job) const;
    bool is_same_config(const JobPtr &job) const;

    void get_not_allowed_machines_when_expanding(const JobPtr &job, IntervalSet &reserved_machines);
    void map_solution_to_static_schedule(const Solution &solution);
    void execute_jobs_on_batsim(double date);
    void inverse_assigned_io_res_units(const JobPtr &job, double date);
    void inverse_assigned_compute_res_units(const JobPtr &job, double date);
    bool check_job_new_reconfiguration(const JobPtr &job);

    Queue<std::string, Job>::SortableElementIterator remove_job_form_execution_queue(const std::string &id);
    Queue<std::string, Job>::SortableElementIterator remove_job_from_waiting_queue(const std::string &id);

    void expand_job_by_given_nb_compute_res_units(const JobPtr &job, double date);
    void shrink_job_by_given_nb_compute_res_units(const JobPtr &job, double date);
    void keep_current_compute_res_units(const JobPtr &job);

    void expand_job_by_given_nb_io_res_units(const JobPtr &job, double date);
    void shrink_job_by_given_io_res_units(const JobPtr &job, double date);
    void keep_current_io_units(const JobPtr &job);

    void resume_job_execution_with_same_configuration(const JobPtr &job, double dat);
    void resume_job_execution_after_stage_in(const JobPtr &job, double dat);

    using ResourcePtr = std::shared_ptr<Resource>;

    ResourcePtr compute_resources;
    ResourcePtr io_resources;

  protected:
    bool _debug = true;
    size_t _nb_jobs_submitted = 0;
    size_t _nb_jobs_completed = 0;

    friend class HPCWorkloadSolverCP;
};
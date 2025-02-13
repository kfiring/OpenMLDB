/*
 * Copyright 2021 4Paradigm
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HYBRIDSE_SRC_PLAN_PLANNER_H_
#define HYBRIDSE_SRC_PLAN_PLANNER_H_

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "base/fe_status.h"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "node/node_manager.h"
#include "node/plan_node.h"
#include "node/sql_node.h"
#include "proto/fe_type.pb.h"
namespace hybridse {
namespace plan {

using base::Status;
using node::NodePointVector;
using node::PlanNode;
using node::PlanNodeList;
using node::SqlNode;

class Planner {
 public:
    Planner(node::NodeManager *manager, const bool is_batch_mode, const bool is_cluster_optimized,
            const bool enable_batch_window_parallelization,
            const std::unordered_map<std::string, std::string>* extra_options = nullptr);
    virtual ~Planner() {}
    virtual base::Status CreatePlanTree(const NodePointVector &parser_trees,
                                        PlanNodeList &plan_trees) = 0;  // NOLINT (runtime/references)
    static base::Status TransformTableDef(const std::string &table_name, const NodePointVector &column_desc_list,
                                          type::TableDef *table);
    bool MergeWindows(const std::map<const node::WindowDefNode *, node::ProjectListNode *> &map,
                      std::vector<const node::WindowDefNode *> *windows);

    static int GetPlanTreeLimitCount(node::PlanNode *node);

 protected:
    const bool is_batch_mode_;
    const bool is_cluster_optimized_;
    const bool enable_window_maxsize_merged_;
    const bool enable_batch_window_parallelization_;
    bool ExpandCurrentHistoryWindow(std::vector<const node::WindowDefNode *> *windows);
    bool IsTable(node::PlanNode *node, node::PlanNode **output);
    base::Status ValidateRequestTable(node::PlanNode *node, std::vector<node::PlanNode *> &request_tables);  // NOLINT
    base::Status ValidateOnlineServingOp(node::PlanNode *node);
    base::Status ValidateClusterOnlineTrainingOp(node::PlanNode *node);
    base::Status CheckWindowFrame(const node::WindowDefNode *w_ptr);
    base::Status CreateQueryPlan(const node::QueryNode *root, PlanNode **plan_tree);
    base::Status CreateSelectQueryPlan(const node::SelectQueryNode *root, PlanNode **plan_tree);
    base::Status CreateUnionQueryPlan(const node::UnionQueryNode *root, PlanNode **plan_tree);
    base::Status CreateCreateTablePlan(const node::SqlNode *root, node::PlanNode **output);
    base::Status CreateTableReferencePlanNode(const node::TableRefNode *root, node::PlanNode **output);
    base::Status CreateCmdPlan(const SqlNode *root, node::PlanNode **output);
    base::Status CreateInsertPlan(const SqlNode *root, node::PlanNode **output);
    base::Status CreateExplainPlan(const SqlNode *root, node::PlanNode **output);
    base::Status CreateCreateIndexPlan(const SqlNode *root, node::PlanNode **output);
    base::Status CreateFuncDefPlan(const SqlNode *root, node::PlanNode **output);
    base::Status CreateWindowPlanNode(const node::WindowDefNode *w_ptr, node::WindowPlanNode *plan_node);
    base::Status CreateDeployPlanNode(const node::DeployNode *root, node::PlanNode **output);
    base::Status CreateLoadDataPlanNode(const node::LoadDataNode *root, node::PlanNode **output);
    base::Status CreateSelectIntoPlanNode(const node::SelectIntoNode *root, node::PlanNode **output);
    base::Status CreateSetPlanNode(const node::SetNode *root, node::PlanNode **output);
    base::Status CreateCreateFunctionPlanNode(const node::CreateFunctionNode *root, node::PlanNode **output);
    base::Status CreateCreateProcedurePlan(const node::SqlNode *root, const PlanNodeList &inner_plan_node_list,
                                           node::PlanNode **output);
    node::NodeManager *node_manager_;

    std::string MakeTableName(const PlanNode *node) const;
    base::Status MergeProjectMap(const std::map<const node::WindowDefNode *, node::ProjectListNode *> &map,
                                 std::map<const node::WindowDefNode *, node::ProjectListNode *> *output);

 private:
    const std::unordered_map<std::string, std::string>* extra_options_ = nullptr;
    std::set<std::string> long_windows_;
};

class SimplePlanner : public Planner {
 public:
    explicit SimplePlanner(node::NodeManager *manager) : Planner(manager, true, false, false) {}
    SimplePlanner(node::NodeManager *manager, bool is_batch_mode, bool is_cluster_optimized = false,
                  bool enable_batch_window_parallelization = true,
                  const std::unordered_map<std::string, std::string>* extra_options = nullptr)
        : Planner(manager, is_batch_mode, is_cluster_optimized, enable_batch_window_parallelization, extra_options) {}
    ~SimplePlanner() {}
    base::Status CreatePlanTree(const NodePointVector &parser_trees,
                                PlanNodeList &plan_trees);  // NOLINT
};

}  // namespace plan
}  // namespace hybridse

#endif  // HYBRIDSE_SRC_PLAN_PLANNER_H_

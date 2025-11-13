#pragma once

#include "ir.hpp"
#include "pass.hpp"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class DeSSAPass : public FunctionPass {
  public:
    bool run(IRFunction &F) override {
        std::cout << "Running DeSSAPass on function: " << F.name << std::endl;
        bool ir_changed = false;

        std::unordered_map<std::string, IRBasicBlock *> label_map;
        for (auto &block : F.blocks) {
            label_map[block->label] = block.get();
        }

        std::unordered_map<IRBasicBlock *, std::vector<std::pair<IROperand, IROperand>>>
            pending_copies;

        std::unordered_set<IRInstruction *> phis_to_delete;

        for (auto &block : F.blocks) {
            for (auto &inst : block->insts) {
                if (inst.op == IROp::LABEL) {
                    continue;
                }
                if (inst.op != IROp::PHI) {
                    break;
                }

                ir_changed = true;
                IROperand dest = inst.result.value();

                for (size_t i = 0; i < inst.args.size(); i += 2) {
                    IROperand src = inst.args[i];
                    std::string label_name = inst.args[i + 1].name;
                    IRBasicBlock *pred_block = label_map.at(label_name);

                    // 收集 (dest, src) 对
                    pending_copies[pred_block].push_back({ dest, src });
                }

                phis_to_delete.insert(&inst);
            }
        }

        if (!ir_changed) return false;

        for (auto &[pred_block, copies] : pending_copies) {

            std::vector<IRInstruction> stage1_moves; // src -> temp
            std::vector<IRInstruction> stage2_moves; // temp -> dest

            for (const auto &[dest, src] : copies) {
                IROperand temp = F.new_reg(src.type);
                stage1_moves.emplace_back(IROp::MOVE, std::vector{ src }, temp);
                stage2_moves.emplace_back(IROp::MOVE, std::vector{ temp }, dest);
            }

            auto terminator_it = std::find_if(
                pred_block->insts.begin(), pred_block->insts.end(), [](const IRInstruction &inst) {
                    return inst.op == IROp::RET || inst.op == IROp::BR || inst.op == IROp::BRZ ||
                           inst.op == IROp::BRLT || inst.op == IROp::BRGT;
                });

            // 阶段 1 (读)
            auto insert_pos = pred_block->insts.insert(terminator_it, stage1_moves.begin(),
                                                       stage1_moves.end());

            // 阶段 2 (写)
            std::advance(insert_pos, stage1_moves.size());
            pred_block->insts.insert(insert_pos, stage2_moves.begin(), stage2_moves.end());
        }

        for (auto &block : F.blocks) {
            auto &insts = block->insts;
            insts.remove_if([&](IRInstruction &inst) { return phis_to_delete.contains(&inst); });
        }

        return true;
    }
};

#pragma once


namespace Emulator {
namespace Ptx {

    void regInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc) {
        wc->thread_regs[lid][reg_] = RegisterContext(count_);
    }

    void pragmaInstruction::ExecuteWarp(std::shared_ptr<WarpContext>& wc) {
        wc->pc+=0;
    }

    void braInstruction::ExecuteBranch(std::shared_ptr<WarpContext>& wc){
        auto mask = wc->GetPredicateMask(prd_.reg_id);
        uint64_t branch_mask = mask & wc->execution_mask;
        if (branch_mask == wc->execution_mask) {
            wc->gotoBasicBlock(sym_);
        } else if (branch_mask == 0) {
            wc->pc += 1;
        } else {
            wc->execution_mask ^= branch_mask;
            wc->execution_stack.push({wc->pc + 1, branch_mask});
            wc->gotoBasicBlock(sym_);
        }
    }

    void retInstruction::ExecuteBranch(std::shared_ptr<WarpContext>& wc){
        if (wc->execution_stack.empty()) {
            wc->pc = WarpContext::EOC;

        } else {
            auto [pc, mask] = wc->execution_stack.top();
            wc->pc = pc;
            wc->execution_mask = mask;
            wc->execution_stack.pop();
        }
    }

    template<dataType DATA>
    void setpInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc){
        wc->pc+=0;
		lid += 0;
    }

    template<dataType DATA>
    void addInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc){
        wc->pc+=0;
		lid += 0;
    }

    template<dataType DATA>
    void movInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc){
        wc->pc+=0;
		lid += 0;
    }

    template<dataType DATA>
    void shlInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc){
        wc->pc+=0;
		lid += 0;
    }

    template<dataType DATA>
    void andInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc){
        wc->pc+=0;
		lid += 0;
    }

    template<dataType DATA>
    void mulInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc){
        wc->pc+=0;
		lid += 0;
    }

    template<dataType DATA>
    void stInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc){
        wc->pc+=0;
		lid += 0;
    }

    template<dataType DATA>
    void negInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc){
        wc->pc+=0;
		lid += 0;
    }

    template<dataType DATA>
    void subInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc){
        wc->pc+=0;
		lid += 0;
    }

    template<dataType DATA>
    void ldInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc){
        wc->pc+=0;
		lid += 0;
    }

    template<dataType DATA>
    void madInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc){
        wc->pc+=0;
		lid += 0;
    }

    template<dataType SRC_DATA, dataType DST_DATA>
    void cvtInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc){
        wc->pc+=0;
		lid += 0;
    }

    template<dataType DATA>
    void cvtaInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc){
        wc->pc+=0;
        lid += 0;
    }

    void labelInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc){
        wc->pc+=0;
		lid += 0;
    }

}
}
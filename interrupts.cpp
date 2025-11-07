/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 *
 */

#include "interrupts.hpp"

std::tuple<std::string, std::string, int> simulate_trace(std::vector<std::string> trace_file, int time, std::vector<std::string> vectors, std::vector<int> delays, std::vector<external_file> external_files, PCB current, std::vector<PCB> wait_queue) {

    std::string trace;      //!< string to store single line of trace file
    std::string execution = "";  //!< string to accumulate the execution output
    std::string system_status = "";  //!< string to accumulate the system status output
    int current_time = time;

    //parse each line of the input trace file. 'for' loop to keep track of indices.
    for(size_t i = 0; i < trace_file.size(); i++) {
        auto trace = trace_file[i];

        auto [activity, duration_intr, program_name] = parse_trace(trace);

        if(activity == "CPU") { //As per Assignment 1
            execution += simulate_cpu(duration_intr, current_time);
        } else if(activity == "SYSCALL") { //As per Assignment 1
            execution += handle_interrupt(duration_intr, current_time, vectors, delays, "SYSCALL ISR");
        } else if(activity == "END_IO") {
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            current_time = time;
            execution += intr;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR(ADD STEPS HERE)\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "FORK") {
            auto [intr, time] = intr_boilerplate(current_time, 2, 10, vectors);
            execution += intr;
            current_time = time;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //FORK implementation
            
            // Count PIDs including current process
            unsigned int child_pid = 1;
            for (const auto& pcb : wait_queue) {
                if (pcb.PID >= child_pid) child_pid = pcb.PID + 1;
            }
            if (current.PID >= child_pid) child_pid = current.PID + 1;
            
            int child_partition = find_available_partition(current.size, wait_queue);

            if(child_partition == -1) {
                execution += std::to_string(current_time) + ", FORK ERROR: No available partition\n";
            } else {

                // Create child PCB with SAME program name as parent
                PCB child(child_pid, current.PID, current.program_name, current.size, child_partition);
                
                execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", cloning the PCB\n";
                memory[child_partition - 1].code = current.program_name;
                current_time += duration_intr;

                execution += std::to_string(current_time) + ", 0, scheduler called\n";
                execution += std::to_string(current_time) + ", 1, IRET\n";
                current_time += 1;

                // Display status
                system_status += "time: " + std::to_string(current_time) + "; current trace: FORK, " 
                        + std::to_string(duration_intr) + "\n";
                system_status += "+------------------------------------------------------+\n";
                system_status += "| PID |program name |partition number | size |   state |\n";
                system_status += "+------------------------------------------------------+\n";

                // Child
                system_status += "|   " + std::to_string(child_pid) + " |    " + child.program_name 
                        + " |               " + std::to_string(child_partition) + " |    " 
                        + std::to_string(child.size) + " | running |\n";

                // Current process (parent)
                system_status += "|   " + std::to_string(current.PID) + " |    " + current.program_name 
                                + " |               " + std::to_string(current.partition_number) + " |    " 
                                + std::to_string(current.size) + " | waiting |\n";

                // Display other waiting queue PCBs
                for (const auto& program : wait_queue) {
                    system_status += "|   " + std::to_string(program.PID) + " |    " + program.program_name 
                            + " |               " + std::to_string(program.partition_number) + " |    " 
                            + std::to_string(program.size) + " | waiting |\n";
                }
                system_status += "+------------------------------------------------------+\n\n";
            }

            ///////////////////////////////////////////////////////////////////////////////////////////

            //The following loop helps you do 2 things:
            // * Collect the trace of the child (and only the child, skip parent)
            // * Get the index of where the parent is supposed to start executing from
            std::vector<std::string> child_trace;
            bool skip = true;
            bool exec_flag = false;
            int parent_index = 0;

            for(size_t j = i; j < trace_file.size(); j++) {
                auto [_activity, _duration, _pn] = parse_trace(trace_file[j]);
                if(skip && _activity == "IF_CHILD") {
                    skip = false;
                    continue;
                } else if(_activity == "IF_PARENT"){
                    skip = true;
                    parent_index = j;
                    if(exec_flag) {
                        break;
                    }
                } else if(skip && _activity == "ENDIF") {
                    skip = false;
                    continue;
                } else if(!skip && _activity == "EXEC") {
                    skip = true;
                    child_trace.push_back(trace_file[j]);
                    exec_flag = true;
                }

                if(!skip) {
                    child_trace.push_back(trace_file[j]);
                }
            }
            i = parent_index;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the child's trace, run the child (HINT: think recursion)
            
            if(child_partition != -1) {
                // Create child PCB
                PCB child(child_pid, current.PID, current.program_name, current.size, child_partition);
                
                // Pass current process in wait_queue for child's execution
                std::vector<PCB> child_wait_queue = wait_queue;
                child_wait_queue.push_back(current);
                
                auto [child_execution, child_status, new_time] = simulate_trace(child_trace, current_time, vectors, delays, external_files, child, child_wait_queue);
                execution += child_execution;
                system_status += child_status;
                current_time = new_time;

                memory[child_partition - 1].code = "empty";
            }

            ///////////////////////////////////////////////////////////////////////////////////////////

        } else if(activity == "EXEC") {
            auto [intr, time] = intr_boilerplate(current_time, 3, 10, vectors);
            current_time = time;
            execution += intr;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //EXEC implementation
            
            unsigned int exec_size = get_program_size(program_name, external_files);

            if (exec_size == 0) {
                execution += std::to_string(current_time) + ", EXEC ERROR: Program not found\n";
            } else {
                int avail_exec_partition = find_available_partition(exec_size, wait_queue);

                if (avail_exec_partition == -1) {
                    execution += std::to_string(current_time) + ", EXEC ERROR: No available partition\n";
                } else {
                    execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) 
                                + ", Program is " + std::to_string(exec_size) + " Mb large\n";
                    current_time += duration_intr;

                    execution += std::to_string(current_time) + ", " + std::to_string(exec_size * 15) 
                                + ", loading program into memory\n";
                    current_time += (exec_size * 15);

                    execution += std::to_string(current_time) + ", 3, marking partition as occupied\n";
                    current_time += 3;

                    execution += std::to_string(current_time) + ", 6, updating PCB\n";
                    current_time += 6;

                    // Free old partition and mark new partition
                    memory[current.partition_number - 1].code = "empty";
                    memory[avail_exec_partition - 1].code = program_name;

                    execution += std::to_string(current_time) + ", 0, scheduler called\n";
                    execution += std::to_string(current_time) + ", 1, IRET\n";
                    current_time += 1;

                    // OUTPUT STATUS HERE (before recursion)
                    system_status += "time: " + std::to_string(current_time) + "; current trace: EXEC, " 
                                    + std::to_string(duration_intr) + "\n";
                    system_status += "+------------------------------------------------------+\n";
                    system_status += "| PID |program name |partition number | size |   state |\n";
                    system_status += "+------------------------------------------------------+\n";

                    // Show new exec program as running
                    system_status += "|   " + std::to_string(current.PID) + " |    " + program_name 
                            + " |               " + std::to_string(avail_exec_partition) + " |    " 
                            + std::to_string(exec_size) + " | running |\n";

                    // Show waiting processes
                    for (const auto& pcb : wait_queue) {
                        system_status += "|   " + std::to_string(pcb.PID) + " |    " + pcb.program_name 
                                + " |               " + std::to_string(pcb.partition_number) + " |    " 
                                + std::to_string(pcb.size) + " | waiting |\n";
                    }
                    system_status += "+------------------------------------------------------+\n\n";

                    ///////////////////////////////////////////////////////////////////////////////////////////

                    // Load exec trace file
                    std::ifstream exec_trace_file(program_name + ".txt");
                    std::vector<std::string> exec_traces;
                    std::string exec_trace;
                    while(std::getline(exec_trace_file, exec_trace)) {
                        exec_traces.push_back(exec_trace);
                    }

                    ///////////////////////////////////////////////////////////////////////////////////////////
                    //With the exec's trace (i.e. trace of external program), run the exec (HINT: think recursion)

                    // Run exec with updated PCB
                    PCB exec_pcb(current.PID, current.PPID, program_name, exec_size, avail_exec_partition);
                    
                    // Remove current process from wait_queue
                    std::vector<PCB> exec_wait_queue;
                    for (const auto& pcb : wait_queue) {
                        if (pcb.PID != current.PID) {
                            exec_wait_queue.push_back(pcb);
                        }
                    }
                    
                    auto [exec_execution, exec_status, exec_time] = simulate_trace(exec_traces, current_time, vectors, delays, external_files, exec_pcb, exec_wait_queue);
                    execution += exec_execution;
                    system_status += exec_status;
                    current_time = exec_time;
                    
                    memory[avail_exec_partition - 1].code = "empty";

                    ///////////////////////////////////////////////////////////////////////////////////////////
                }
            }

            break; //Why is this important? (answer in report)
        }
    }

    return {execution, system_status, current_time};
}

int main(int argc, char** argv) {

    //vectors is a C++ std::vector of strings that contain the address of the ISR
    //delays  is a C++ std::vector of ints that contain the delays of each device
    //the index of these elements is the device number, starting from 0
    //external_files is a C++ std::vector of the struct 'external_file'. Check the struct in 
    //interrupt.hpp to know more.
    auto [vectors, delays, external_files] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    //Just a sanity check to know what files you have
    print_external_files(external_files);

    //Make initial PCB (notice how partition is not assigned yet)
    PCB current(0, -1, "init", 1, -1);
    //Update memory (partition is assigned here, you must implement this function)
    if(!allocate_memory(&current)) {
        std::cerr << "ERROR! Memory allocation failed!" << std::endl;
    }

    std::vector<PCB> wait_queue;

    /******************ADD YOUR VARIABLES HERE*************************/



    /******************************************************************/

    //Converting the trace file into a vector of strings.
    std::vector<std::string> trace_file;
    std::string trace;
    while(std::getline(input_file, trace)) {
        trace_file.push_back(trace);
    }

    auto [execution, system_status, _] = simulate_trace(trace_file, 
                                            0, 
                                            vectors, 
                                            delays,
                                            external_files, 
                                            current, 
                                            wait_queue);

    input_file.close();

    write_output(execution, "execution.txt");
    write_output(system_status, "system_status.txt");

    return 0;
}
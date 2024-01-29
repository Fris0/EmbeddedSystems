
/* Program: Governor_final.cpp
 * Authors: Z. Li (SN: 14070308) and M.F. Jansen (SN: 13385569)
 *
 * Summary:
 * This program functions as a Governor for the Khadas VIM3 board. 
 * The Governor is designed to work as a Proportional Integral Derivative Controller with
 * as goal to reduce the Power Consumption and meeting the Latency and FPS goals set by the user.
 * It achieves this by distributing the layers with the PipeALL library and 
 * increasing/reducing the frequencies of the CPU components.
 * 
 * Instructions to Run
 * On Your Computer:
 * arm-linux-androideabi-clang++ -static-libstdc++ Governor_final.cpp PID_controller.cpp frequency_scaling.cpp -o Governor_final
 * adb push Governor_final /data/local/Working_dir
 *
 * On the Board:
 * chmod +x Governor_final
 * ./Governor_final graph_alexnet_all_pipe_sync #NumberOFPartitions #TargetFPS #TargetLatency
*/

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include "PID_controller.h"
#include "frequency_scaling.h"
using namespace std;

int LittleFrequencyTable[] = {500000, 667000, 1000000, 1200000, 1398000, 1512000, 1608000, 1704000, 1800000};
int BigFrequencyTable[] = {500000, 667000, 1000000, 1200000, 1398000, 1512000, 1608000, 1704000, 1800000};

int LittleFrequencyCounter = 0;
int BigFrequencyCounter = 0;

int MaxLittleFrequencyCounter = 8;
int MaxBigFrequencyCounter = 8;

bool LatencyCondition = 0;
bool FPSCondition = 0;
bool ReduceBigFrequency = 1;
bool ReduceLittleFrequency = 1;

float StageOneInferenceTime = 0;
float StageTwoInferenceTime = 0;
float StageThreeInferenceTime = 0;

int partitions = 0;
int Target_FPS = 0;
int Target_Latency = 0;
float Actual_FPS = 0;
float Actual_Latency = 0;

// PID controller gain components.
float K_P = 2.0;
float K_I = 0.0;
float K_D = 0.09;


/* Parse the results from the output.txt file and
 * store values in the Global Variables.
 *
 * Output: None
 * Side-effect: Print parsed values to standard output,
 * 				and store new values in global variables. 
*/
void ParseResults()
{
	ifstream myfile("output.txt");
	cout << endl;
	/* Read Output.txt File and Extract Data */
	for (std::string line; getline(myfile, line);)
	{
		string temp;
		/* Extract Frame Rate */
		if (line.find("Frame rate is:") == 0)
		{
			// cout<<"line is: "<<line<<std::endl;
			std::istringstream ss(line);
			while (!ss.eof())
			{
				/* extracting word by word from stream */
				ss >> temp;
				/* Checking the given word is float or not */
				if (stringstream(temp) >> Actual_FPS)
				{
					printf("Throughput is: %f FPS\n", Actual_FPS);
					break;
				}
				temp = "";
			}
		}
		/* Extract Frame Latency */
		if (line.find("Frame latency is:") == 0)
		{
			// cout<<"line is: "<<line<<std::endl;
			std::istringstream ss(line);
			while (!ss.eof())
			{
				/* extracting word by word from stream */
				ss >> temp;
				/* Checking the given word is float or not */
				if (stringstream(temp) >> Actual_Latency)
				{
					printf("Latency is: %f ms\n", Actual_Latency);
					break;
				}
				temp = "";
			}
		}
		/* Extract Stage One Inference Time */
		if (line.find("stage1_inference_time:") == 0)
		{
			// cout<<"line is: "<<line<<std::endl;
			std::istringstream ss(line);
			while (!ss.eof())
			{
				/* extracting word by word from stream */
				ss >> temp;
				/* Checking the given word is float or not */
				if (stringstream(temp) >> StageOneInferenceTime)
				{
					// printf("StageOneInferenceTime is: %f ms\n", StageOneInferenceTime);
					break;
				}
				temp = "";
			}
		}
		/* Extract Stage Two Inference Time */
		if (line.find("stage2_inference_time:") == 0)
		{
			// cout<<"line is: "<<line<<std::endl;
			std::istringstream ss(line);
			while (!ss.eof())
			{
				/* extracting word by word from stream */
				ss >> temp;
				/* Checking the given word is float or not */
				if (stringstream(temp) >> StageTwoInferenceTime)
				{
					// printf("StageTwoInferenceTime is: %f ms\n", StageTwoInferenceTime);
					break;
				}
				temp = "";
			}
		}
		/* Extract Stage Three Inference Time */
		if (line.find("stage3_inference_time:") == 0)
		{
			// cout<<"line is: "<<line<<std::endl;
			std::istringstream ss(line);
			while (!ss.eof())
			{
				/* extracting word by word from stream */
				ss >> temp;
				/* Checking the given word is float or not */
				if (stringstream(temp) >> StageThreeInferenceTime)
				{
					// printf("StageThreeInferenceTime is: %f ms\n", StageThreeInferenceTime);
					break;
				}
				temp = "";
			}
		}
	}
	/* Check Throughput and Latency Constraints */
	if (Actual_Latency <= Target_Latency)
	{
		LatencyCondition = 1; // Latency requirement was met.
	}
	else
	{
		// Set LatencyCondition for frequency reduction.
		LatencyCondition = 0;
	}

	if (Actual_FPS >= Target_FPS)
	{
		FPSCondition = 1; // FPS requirement was met.	
	}
	else
	{
		// Set FPSCondition for frequency reduction.
		FPSCondition = 0;
	}
}

/* Reduce the Big and Lil frequencies until diminishing returns.
 *
 * Graph: String that contains the CNN to be used.
 * N_Frames: Number of frames to be proccesed by CNN.
 * PartitionPoint1: Partitionpoint for first stage.
 * PartitionPoint2:Partitionpoint for second stage.
 * Order: Order of the three processing units Lil, Big cpu and GPU.
 * 
 * Output: None
 * Side-effects: Change the frequencies in Khadas through shell,
 *               print information to standard output, and call
 *               the parse function.
*/
void optimize_frequency(string graph, int N_Frames, int PartitionPoint1, int PartitionPoint2, string Order)
{
	while (true)
	{
		if (BigFrequencyCounter > 0)
		{
			BigFrequencyCounter -= 1;
			string Command = "echo " + to_string(BigFrequencyTable[BigFrequencyCounter]) + " > /sys/devices/system/cpu/cpufreq/policy2/scaling_max_freq";
			system(Command.c_str());
			printf("Decreasing Frequency of Big Cores to %d\n", BigFrequencyTable[BigFrequencyCounter]);

			char Run_Command[150];
			sprintf(Run_Command, "./%s --threads=4 --threads2=2 --target=NEON --n=%d --partition_point=%d --partition_point2=%d --order=%s > output.txt",
			        graph.c_str(), N_Frames, PartitionPoint1, PartitionPoint2, Order.c_str());
			system(Run_Command);
			ParseResults();
		}
		else
		{
			break;
		}

        // Return to previous frequency when reduction caused negative effects.
		if (!FPSCondition || !LatencyCondition)
		{
			BigFrequencyCounter += 1;
			FPSCondition = 1;
			LatencyCondition = 1;
			break;
		}
	}

	while (true)
	{
		if (LittleFrequencyCounter > 0)
		{
			LittleFrequencyCounter -= 1;
			string Command = "echo " + to_string(LittleFrequencyTable[LittleFrequencyCounter]) + " > /sys/devices/system/cpu/cpufreq/policy0/scaling_max_freq";
			system(Command.c_str());
			printf("Decreasing Frequency of Little Cores to %d\n", LittleFrequencyTable[LittleFrequencyCounter]);

			char Run_Command[150];
			sprintf(Run_Command, "./%s --threads=4 --threads2=2 --target=NEON --n=%d --partition_point=%d --partition_point2=%d --order=%s > output.txt",
			        graph.c_str(), N_Frames, PartitionPoint1, PartitionPoint2, Order.c_str());
			system(Run_Command);
			ParseResults();
		}
		else
		{
			break;
		}

        // Return to previous frequency when reduction caused negative effects.
		if (!FPSCondition || !LatencyCondition)
		{
			LittleFrequencyCounter += 1;
			FPSCondition = 1;
			LatencyCondition = 1;
			break;
		}
	}
}

/* Run the sequential steps of the Governor design to obtain the optimal
 * PipeALL configuration.
 *
 * argc: Count of arguments sent through command line.
 * argv: The actual argument strings.
 * 
 * Output: 0, to conclude and define the end of the program.
 *
 * Side-effects: Run commands in the systems command-line
 *               for testing and changing the configuration,
 *               printing intermediate values, and change
 *               the global values.
*/
int main(int argc, char *argv[])
{
	if (argc < 5)
	{
		std::cout << "Wrong number of input arguments.\n";
		return -1;
	}
	string graph = argv[1];
	partitions = atoi(argv[2]);
	Target_FPS = atoi(argv[3]);
	Target_Latency = atoi(argv[4]);

	string Command = "";

	/* Checking if processor is available */
	if (system(NULL))
		puts("Ok");
	else
		exit(EXIT_FAILURE);

	/* Export OpenCL library path */
	system("export LD_LIBRARY_PATH=/data/local/Working_dir");
	setenv("LD_LIBRARY_PATH", "/data/local/Working_dir", 1);

	/* Setup Performance Governor (CPU) */
	system("echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor");
	system("echo performance > /sys/devices/system/cpu/cpufreq/policy2/scaling_governor");

	/* Initialize Little and Big CPU with Lowest Frequency */
	Command = "echo " + to_string(LittleFrequencyTable[0]) + " > /sys/devices/system/cpu/cpufreq/policy0/scaling_max_freq";
	system(Command.c_str());
	Command = "echo " + to_string(BigFrequencyTable[0]) + " > /sys/devices/system/cpu/cpufreq/policy2/scaling_max_freq";
	system(Command.c_str());

	int N_Frames = 10;
	int PartitionPoint1 = 1;
	int PartitionPoint2 = 1;
	string Order = "B-G-L";

	while (true)
	{
		char Run_Command[150];
		sprintf(Run_Command, "./%s --threads=4 --threads2=2 --target=NEON --n=%d --partition_point=%d --partition_point2=%d --order=%s > output.txt",
				graph.c_str(), N_Frames, PartitionPoint1, PartitionPoint2, Order.c_str());
		system(Run_Command);
		ParseResults();

		if (FPSCondition && LatencyCondition)
		{
			printf("Temporary solution Was Found.\n TargetBigFrequency:%d \t TargetLittleFrequency:%d \t PartitionPoint1:%d \t PartitionPoint2:%d \t Order:%s\n",
				   BigFrequencyTable[BigFrequencyCounter], LittleFrequencyTable[LittleFrequencyCounter], PartitionPoint1, PartitionPoint2, Order.c_str());
			printf("Now trying to reduce frequency.\n\n");
			optimize_frequency(graph, N_Frames, PartitionPoint1, PartitionPoint2, Order);
			printf("Final solution Was Found.\n TargetBigFrequency:%d \t TargetLittleFrequency:%d \t PartitionPoint1:%d \t PartitionPoint2:%d \t Order:%s\n",
				   BigFrequencyTable[BigFrequencyCounter], LittleFrequencyTable[LittleFrequencyCounter], PartitionPoint1, PartitionPoint2, Order.c_str());

			break;
		}
		else
		{
			// Create aggregated value for the desired and actual values.
			float desired_value = !FPSCondition * (1.0 / Target_FPS * 1000) + !LatencyCondition * Target_Latency;
			float actual_value = !FPSCondition * (1.0 / Actual_FPS * 1000) + !LatencyCondition * Actual_Latency;

			// Calculate the Controller Output with the Aggregated values.
			float controller_out = controller_output(desired_value, actual_value, Actual_Latency, K_P, K_I, K_D);
			printf("Partition: %i, %i\n", PartitionPoint1, PartitionPoint2);
			printf("Desired value: %f, Actual value: %f\n", desired_value, actual_value);
			printf("PID controller output: %f\n", controller_out);

			printf("Target Performance Not Satisfied\n\n");

			if (LittleFrequencyCounter < MaxLittleFrequencyCounter)
			{
				/* Push Frequency of Little Cluster Higher to Meet Target Performance */
				LittleFrequencyCounter = freq_upscaling(controller_out, desired_value, MaxLittleFrequencyCounter, LittleFrequencyCounter);
				Command = "echo " + to_string(LittleFrequencyTable[LittleFrequencyCounter]) + " > /sys/devices/system/cpu/cpufreq/policy0/scaling_max_freq";
				system(Command.c_str());
				printf("Increasing Frequency of Little Cores to %d\n", LittleFrequencyTable[LittleFrequencyCounter]);
			}
			else
			{
				if (BigFrequencyCounter < MaxBigFrequencyCounter)
				{
					/* Push Frequency of Small Cluster Higher to Meet Target Performance */
					BigFrequencyCounter = freq_upscaling(controller_out, desired_value, MaxBigFrequencyCounter, BigFrequencyCounter);;
					Command = "echo " + to_string(BigFrequencyTable[BigFrequencyCounter]) + " > /sys/devices/system/cpu/cpufreq/policy2/scaling_max_freq";
					system(Command.c_str());
					printf("Increasing Frequency of Big Cores to %d\n", BigFrequencyTable[BigFrequencyCounter]);
				}
				else
				{
					if (StageTwoInferenceTime < StageThreeInferenceTime)
					{
						if (PartitionPoint2 < partitions)
						{
							PartitionPoint2 += 1;
							printf("Decreasing the Size of Pipeline Partition 3\n");
							printf("Increasing the Size of Pipeline Partition 2\n");
						}
						else
						{
							printf("No Solution Found\n");
							break;
						}
					}
					else if (StageOneInferenceTime < StageTwoInferenceTime)
					{
						if (PartitionPoint2 > PartitionPoint1)
						{
							PartitionPoint1 += 1;
							printf("Increasing the Size of Pipeline Partition 1\n");
						}
						else
						{
							printf("No Solution Found\n");
							break;
						}
					}
				}
			}
		}
	}

	return 0;
}

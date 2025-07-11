#include <iostream>
#include "tsp_ga.h"
#include "population.h"
#include "mutation.h"
#include <array>
#include <algorithm>
#include <random>
#include <tuple>
#include <list>
#include <chrono>

TspGaConfig TspGa::config;
DistanceHelper TspGa::distanceHelper;
Chromosome TspGa::bestChromosome;
auto elapsedTimeMins = 0;
auto elapsedTimeSecs = 0;
auto elapsedTimeMs = 0;

TspGa::TspGa(){
    // Constructor
}

auto TspGa::GetPoints() -> std::vector<Point>&{
    return points;
}

void TspGa::InitPopulation(){
    std::cout << "Initializing Population" << std::endl;

    this->points = fileReader.ParseCsv("../data/tsp81cities_coords.csv");
    
    config.chromosomeSize = static_cast<unsigned int>(points.size());

    distanceHelper.CreateDistanceMatrixFromPoints(points);

    std::cout << "Distance between point id 1 and 2: " << distanceHelper.GetDistanceByPointId(1, 2) << std::endl;
    std::cout << "Distance between point id 7 and 18: " << distanceHelper.GetDistanceByPointId(7, 18) << std::endl;

    std::cout << "Distance between point id 55 and 56: " << distanceHelper.GetDistanceByPointId(55, 56) << std::endl;
    std::cout << "Distance between point id 50 and 81: " << distanceHelper.GetDistanceByPointId(50, 81) << std::endl;

    population.GenerateRandomInitialPopulation();
}

// TODO: "Early stopping" criteria can be added to stop the algorithm if the fitness score is not improved for a certain number of generations.
void TspGa::CreateGenerations(Population& parentPopulation){
    auto [createdGenerationCount, maxGenerations] = std::make_tuple(0u, config.maxGenerations);
    float lastFitnessScore;

    auto mutationPatience = 0;
    while (parentPopulation.GetSize() > 4){
        auto executionStartTime = std::chrono::high_resolution_clock::now();

        parentPopulation.SelectBestChromosomes();
        auto parentPopulationSize = parentPopulation.GetSize();

        std::cout << "Best Solution for the Generation " << createdGenerationCount << ": " << parentPopulation.GetChromosome(0).GetFitnessScore() << " with Population Size: " << parentPopulationSize << std::endl;

        if (parentPopulationSize < 4){
            break;
        }
        
        //Mutation
        float last = parentPopulation.GetChromosome(0).GetFitnessScore();
        float prev = lastFitnessScore;
        
        mutationPatience = (last == prev) ? mutationPatience + 1 : 0;

        if (mutationPatience >= config.mutationPatience){
            std::cout << "Algorithm is stuck in a local minima. Mutation condition is met..." << std::endl;
            parentPopulation.Mutate();
            parentPopulation.CalculateFitnessScores();
        }
        lastFitnessScore = parentPopulation.GetChromosome(0).GetFitnessScore();

        parentPopulation = parentPopulation.GenerateSubPopulation(CrossoverStrategy::ShuffledSequentialPair);
        parentPopulation.CalculateFitnessScores();

        auto executionEndTime = std::chrono::high_resolution_clock::now();
        auto executionDuration = std::chrono::duration_cast<std::chrono::milliseconds>(executionEndTime - executionStartTime);
        std::cout << "Execution time: " << executionDuration.count() << " milliseconds" << std::endl;
        elapsedTimeMs += executionDuration.count();

        createdGenerationCount++;

        if (createdGenerationCount == maxGenerations){
            std::cout << "Number of maximum generations have been reached." << std::endl << std::endl;
            elapsedTimeMins = elapsedTimeMs / 1000 / 60;
            elapsedTimeSecs = (elapsedTimeMs - elapsedTimeMins*60*1000) / 1000;
            elapsedTimeMs = elapsedTimeMs % 1000;
            std::cout << "Elapsed Time: " << elapsedTimeMins << " mins " << elapsedTimeSecs << " secs " << elapsedTimeMs << " ms" << std::endl;
            std::cout << "Best Solution: " << bestChromosome.GetFitnessScore() <<std::endl;
            bestChromosome.PrintGenes();
            return;
        }
    }

    std::cout << "There aren't enough chromosomes to crossover..." << std::endl;
    elapsedTimeMins = elapsedTimeMs / 1000 / 60;
    elapsedTimeSecs = (elapsedTimeMs - elapsedTimeMins*60*1000) / 1000;
    elapsedTimeMs = elapsedTimeMs % 1000;
    std::cout << "Elapsed Time: " << elapsedTimeMins << " mins " << elapsedTimeSecs << " secs " << elapsedTimeMs << " ms" << std::endl;
    std::cout << "Best Solution: " << bestChromosome.GetFitnessScore() << std::endl;
    bestChromosome.PrintGenes();

    if (!bestChromosome.IsValid()){
        std::cerr << "Best Chromosome is not valid" << std::endl;
    }
}

void TspGa::Solve(){
    CreateGenerations(this->population);

    //TestCrossovers();
}

// TODO: Just for tests, remove this function later.
void TspGa::TestCrossovers(){
    std::cout << "Testing Crossovers" << std::endl;
    
    Population generation;
    population.SelectBestChromosomes();

    std::cout << "Best Chromosomes: " << std::endl;
    for (auto& chromosome : population.GetChromosomes()){
        chromosome.PrintGenes();
        std::cout << std::endl << " Fitness Score: " << chromosome.GetFitnessScore() << std::endl;
        std::cout << " -------------------------------------- " << std::endl;
    }

    // Test Crossover for the best two chromosomes
    std::cout << "Applying partially mapped crossover to best two chromosomes" << std::endl;
    auto offSprings = Crossover::ApplyPartiallyMapped(population.GetChromosome(0), population.GetChromosome(1));
    
    std::cout << "Offspring 1: ";
    offSprings.first.PrintGenes();
    std::cout << std::endl;

    std::cout << "Offspring 2: ";
    offSprings.second.PrintGenes();
    std::cout << std::endl;

    // Test for order based crossover
    std::cout << "------------------------------------------------" << std::endl;
    std::cout << "Applying order based crossover to best two chromosomes" << std::endl;
    offSprings = Crossover::ApplyOrderBased(population.GetChromosome(0), population.GetChromosome(1));
    
    std::cout << "Offspring 1: ";
    offSprings.first.PrintGenes();
    std::cout << std::endl;

    std::cout << "Offspring 2: ";
    offSprings.second.PrintGenes();
    std::cout << std::endl;

    // Test for cycle crossover
    std::cout << "------------------------------------------------" << std::endl;
    std::cout << "Applying cycle crossover to best two chromosomes" << std::endl;

    std::cout << "Parent 1: ";
    population.GetChromosome(0).PrintGenes();
    std::cout << std::endl;

    std::cout << "Parent 2: ";
    population.GetChromosome(1).PrintGenes();
    std::cout << std::endl;

    offSprings = Crossover::ApplyCycleBased(population.GetChromosome(0), population.GetChromosome(1));

    std::cout << "Offspring 1: ";
    offSprings.first.PrintGenes();
    std::cout << std::endl;

    std::cout << "Offspring 2: ";
    offSprings.second.PrintGenes();
    std::cout << std::endl;
}
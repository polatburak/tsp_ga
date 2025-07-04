#include "population.h"
#include "tsp_ga.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <tuple>
#include "crossover.h"
#include <functional>
#include <map>
#include "mutation.h"

TspGaConfig tspConfig;

Population::Population(){
    // Constructor
}

Population::Population(const Population& other) : chromosomes(other.chromosomes)
{
}

Population& Population::operator=(const Population& other){
    chromosomes = other.chromosomes;
    return *this;
}

Population& Population::operator=(Population&& other) noexcept{
    chromosomes = std::move(other.chromosomes);
    return *this;
}

void Population::AddChromosome(const Chromosome& chromosome){
    chromosomes.push_back(chromosome);
}

void Population::CalculateFitnessScores(){
    for (auto& chromosome : chromosomes){
        chromosome.CalculateFitnessScore();
    }
}

void Population::ClearPopulation(){
    chromosomes.clear();
}

void Population::GenerateRandomInitialPopulation(){
    this->ClearPopulation();

    auto startPointId = TspGa::config.startPointId;

    Chromosome chromosome;
    chromosome.AddGene(startPointId);
    for (unsigned int geneIndex = 0; geneIndex < TspGa::config.chromosomeSize; geneIndex++){
        auto gene = geneIndex + 1;
        if (gene == startPointId){
            continue;
        }

        // We decided to use id for the points instead of index so we start from 1
        chromosome.AddGene(gene);
    }

    for (unsigned int chromosomeIndex = 0; chromosomeIndex < TspGa::config.initialPopulationSize; chromosomeIndex++){
        chromosome.ShuffleGenes();

        //std::cout << "Chromosome " << chromosomeIndex << ": ";
        //chromosome.PrintGenes();
        //std::cout << std::endl << " -------------------------------------- " << std::endl;

        this->AddChromosome(chromosome);
    }

    this->CalculateFitnessScores();
}

// TODO: Keep also some of the worst chromosomes to maintain diversity
void Population::SelectBestChromosomes(){
    std::sort(chromosomes.begin(), chromosomes.end(), [](const Chromosome& c1, const Chromosome& c2){
        return c1.GetFitnessScore() < c2.GetFitnessScore();
    });

    auto populationSize = GetSize();

    const double bestSize = static_cast<double>(populationSize) * tspConfig.bestChromosomesPct;
    auto firstIndexOfRest = static_cast<decltype(chromosomes.begin())::difference_type>(bestSize);
    std::shuffle(chromosomes.begin() + firstIndexOfRest, chromosomes.end(), std::mt19937(std::random_device{}()));
    
    const double restSize = static_cast<double>(populationSize - bestSize) * tspConfig.restChromosomesPct;
    auto firstIndexToRemove = firstIndexOfRest + static_cast<decltype(chromosomes.begin())::difference_type>(restSize);
    chromosomes.erase(chromosomes.begin() + firstIndexToRemove, chromosomes.end());

    if (TspGa::bestChromosome.GetSize() == 0 || GetChromosome(0).GetFitnessScore() < TspGa::bestChromosome.GetFitnessScore()){
        TspGa::bestChromosome = GetChromosome(0);
    }

    tspConfig.bestChromosomesPct = tspConfig.bestChromosomesPct * (1 - tspConfig.bestChromosomesDecreaseRate);
}

auto Population::GenerateSubPopulation(const CrossoverStrategy& crossoverStrategy) -> Population{
    Population newPopulation;
    auto size = GetSize();

    // Choose the elite chromosomes and pass them to the next generation
    const double eliteSize = static_cast<double>(size) * TspGa::config.eliteChromosomesPct;
    auto lastEliteIndex = static_cast<decltype(chromosomes.begin())::difference_type>(eliteSize);
    std::for_each(chromosomes.begin(), chromosomes.begin() + lastEliteIndex, [&](const Chromosome& chromosome){
        newPopulation.AddChromosome(chromosome);
    });

    // TODO: Try to use std::tuple instead of OffspringPair struct with cpp17+ [Now we get structured bindings error with std::tuple<std::pair<...>, std::pair<...>>]
    struct OffspringPair {
        std::pair<Chromosome, Chromosome> offSprings1;
        std::pair<Chromosome, Chromosome> offSprings2;
    };

    // Partially mapped crossover where 2 crossover happens with each chromosome
    auto createOffspringPairs = [&](const Chromosome& chromosome1, const Chromosome& chromosome2) -> OffspringPair{
        auto offSprings1 = Crossover::ApplyPartiallyMapped(chromosome1, chromosome2);
        auto offSprings2 = Crossover::ApplyPartiallyMapped(chromosome1, chromosome2);

        offSprings1.first.CalculateFitnessScore();
        offSprings1.second.CalculateFitnessScore(); 

        offSprings2.first.CalculateFitnessScore();
        offSprings2.second.CalculateFitnessScore();

        return {offSprings1, offSprings2};
    };

        auto chooseAndAddBetterOffspring = [&](Chromosome& offspring1, Chromosome& offspring2){
            auto& betterOffspring = offspring1.GetFitnessScore() <= offspring2.GetFitnessScore() ? offspring1 : offspring2;
            newPopulation.AddChromosome(betterOffspring);
        };

    auto funcsByStrategy = std::map<CrossoverStrategy, std::function<void()>>{
        {CrossoverStrategy::EveryPair, [&](){
            for (auto i = 0u; i < size-1; i++) {
                for (auto j = i+1; j < size; j++) {
                    auto [offSprings1, offSprings2] = createOffspringPairs(GetChromosome(i), GetChromosome(j));

                    chooseAndAddBetterOffspring(offSprings1.first, offSprings1.second);
                    chooseAndAddBetterOffspring(offSprings2.first, offSprings2.second);
                }
            }
        }},
        {CrossoverStrategy::SequentialPair, [&](){
            for (auto i = 0u; i < size-2; i++) {
                auto [offSprings1, offSprings2] = createOffspringPairs(GetChromosome(i), GetChromosome(i+1));

                chooseAndAddBetterOffspring(offSprings1.first, offSprings1.second);
                chooseAndAddBetterOffspring(offSprings2.first, offSprings2.second);
            }
        }},
        {CrossoverStrategy::ShuffledSequentialPair, [&](){
            Shuffle();
            for (auto i = 0u; i < size-2; i++) {
                auto [offSprings1, offSprings2] = createOffspringPairs(GetChromosome(i), GetChromosome(i+1));

                chooseAndAddBetterOffspring(offSprings1.first, offSprings1.second);
                chooseAndAddBetterOffspring(offSprings2.first, offSprings2.second);
            }
        }}
    };

    funcsByStrategy[crossoverStrategy]();

    return newPopulation;
}

void Population::Shuffle(){
    std::shuffle(chromosomes.begin(), chromosomes.end(), std::mt19937(std::random_device{}()));
}

void Population::Mutate(){
    std::cout << "Mutation Rate: " << tspConfig.mutationRate << std::endl;

    for(auto& chromosome : GetChromosomes()){
        Mutation::ApplyInversion(chromosome);
    }

    tspConfig.mutationRate = tspConfig.mutationRate * (1 + tspConfig.mutationIncreaseRate);
}

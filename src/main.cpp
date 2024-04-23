#define CROW_MAIN
#define CROW_STATIC_DIR "../public"

#include "crow_all.h"
#include "json.hpp"
#include <random>
#include <thread>
#include <condition_variable>
#include <mutex>

static const uint32_t NUM_ROWS = 15;

// Constants
const uint32_t PLANT_MAXIMUM_AGE = 10;
const uint32_t HERBIVORE_MAXIMUM_AGE = 50;
const uint32_t CARNIVORE_MAXIMUM_AGE = 80;
const uint32_t MAXIMUM_ENERGY = 200;
const uint32_t THRESHOLD_ENERGY_FOR_REPRODUCTION = 20;
const uint32_t MOVE_ENERGY = 5;
const uint32_t CARNIVORE_ENERGY_GAIN = 20;
const uint32_t HERBIVORE_ENERGY_GAIN = 30;
const uint32_t REPRODUCTION_ENERGY = 10;
const uint32_t START_ENERGY = 100;


// Probabilities
const double PLANT_REPRODUCTION_PROBABILITY = 0.2;
const double HERBIVORE_REPRODUCTION_PROBABILITY = 0.075;
const double CARNIVORE_REPRODUCTION_PROBABILITY = 0.025;
const double HERBIVORE_MOVE_PROBABILITY = 0.7;
const double HERBIVORE_EAT_PROBABILITY = 0.9;
const double CARNIVORE_MOVE_PROBABILITY = 0.5;
const double CARNIVORE_EAT_PROBABILITY = 1.0;

// Type definitions
enum entity_type_t
{
    empty,
    plant,
    herbivore,
    carnivore
};

struct pos_t
{
    uint32_t i;
    uint32_t j;

	pos_t(int i, int j) : i(i), j(j) {};
	pos_t() {};
    
};

struct entity_t
{
    entity_type_t type;
    int32_t energy;
    int32_t age;
};

// Auxiliary code to convert the entity_type_t enum to a string
NLOHMANN_JSON_SERIALIZE_ENUM(entity_type_t, {
                                                {empty, " "},
                                                {plant, "P"},
                                                {herbivore, "H"},
                                                {carnivore, "C"},
                                            })

// Auxiliary code to convert the entity_t struct to a JSON object
namespace nlohmann
{
    void to_json(nlohmann::json &j, const entity_t &e)
    {
        j = nlohmann::json{{"type", e.type}, {"energy", e.energy}, {"age", e.age}};
    }
}

// Grid that contains the entities
static std::vector<std::vector<entity_t>> entity_grid;

std::default_random_engine gen;
std::uniform_int_distribution<> rand_pos(0, NUM_ROWS - 1);
std::uniform_real_distribution<> mp_rand(0.0, 1.0);

std::vector<std::thread> running_threads;
std::atomic<int> current_it = 0;

std::mutex exec_it_mtx;
std::condition_variable exec_it;
std::atomic<bool> start;

void plant_routine(pos_t pos){
    entity_t& plant = entity_grid[pos.i][pos.j];
    int my_it = current_it;
    std::unique_lock lk(exec_it_mtx);
    while(!start) {
        exec_it.wait(lk);
       }

    if(plant.type == empty or plant.age == PLANT_MAXIMUM_AGE) {
            entity_grid[pos.i][pos.j] = {empty, 0, 0};
            return;
        }
    std::vector<pos_t> empty_pos;
     for(const pos_t& pos_to_verify : {pos_t(pos.i, pos.j+1), pos_t(pos.i,pos.j-1), pos_t(pos.i+1,pos.j), pos_t(pos.i-1,pos.j)}) {

            if(!(pos_to_verify.i >= 0 and pos_to_verify.i < NUM_ROWS and pos_to_verify.j >= 0 and pos_to_verify.j < NUM_ROWS)) {
				continue;
            }
            
            if(entity_grid[pos_to_verify.i][pos_to_verify.j].type == empty)
                empty_pos.push_back(pos_to_verify);
        }
        
        if(mp_rand(gen) < PLANT_REPRODUCTION_PROBABILITY and !empty_pos.empty()) {

            size_t idx = mp_rand(gen) * empty_pos.size();
            std::vector<pos_t>::iterator idx_it = empty_pos.begin() + idx;

            pos_t child_pos = empty_pos[idx];

            entity_grid[child_pos.i][child_pos.j] = {entity_type_t::plant, 0, 0};
           // running_threads.push_back(std::thread(plant_routine, child_pos));
        }
        
        plant.age ++;
}

void herbi_routine(pos_t pos){
    pos_t& cur_pos = pos;
    entity_t herbi ;
    std::unique_lock lk(exec_it_mtx);
    while(!start) {
        exec_it.wait(lk);
     }
    herbi = entity_grid[cur_pos.i][cur_pos.j];
    if(herbi.type == empty or herbi.energy == 0 or herbi.age == HERBIVORE_MAXIMUM_AGE) {
            entity_grid[cur_pos.i][cur_pos.j] = {empty, 0, 0};
            return;
    }

    std::vector<pos_t> empty_pos;
    for(const pos_t& pos_to_verify : {pos_t(cur_pos.i, cur_pos.j+1), pos_t(cur_pos.i,cur_pos.j-1), pos_t(cur_pos.i+1,cur_pos.j), pos_t(cur_pos.i-1,cur_pos.j)}) {

            if(!(pos_to_verify.i >= 0 and pos_to_verify.i < NUM_ROWS and pos_to_verify.j >= 0 and pos_to_verify.j < NUM_ROWS)) {
				continue;
            }

            if(entity_grid[pos_to_verify.i][pos_to_verify.j].type == plant and mp_rand(gen) < HERBIVORE_EAT_PROBABILITY) {
                entity_grid[pos_to_verify.i][pos_to_verify.j] = {empty, 0, 0};
                herbi.energy += HERBIVORE_ENERGY_GAIN;
            }
            
            if(entity_grid[pos_to_verify.i][pos_to_verify.j].type == empty)
                empty_pos.push_back(pos_to_verify);
        }

        if(mp_rand(gen) < HERBIVORE_REPRODUCTION_PROBABILITY and herbi.energy > THRESHOLD_ENERGY_FOR_REPRODUCTION and !empty_pos.empty()) {
            size_t idx = mp_rand(gen) * empty_pos.size();
            std::vector<pos_t>::iterator idx_it = empty_pos.begin() + idx;
            pos_t child_pos = empty_pos[idx];
            if(entity_grid[child_pos.i][child_pos.j].type == empty){
                entity_grid[child_pos.i][child_pos.j] = {entity_type_t::herbivore, START_ENERGY, 0};
                //running_threads.push_back(std::thread(herbi_routine, child_pos));
                empty_pos.erase(idx_it);
                herbi.energy -= REPRODUCTION_ENERGY;
        }
        }

        if(mp_rand(gen) < HERBIVORE_MOVE_PROBABILITY and !empty_pos.empty()) {
            size_t idx = mp_rand(gen) * empty_pos.size();
            pos_t next_pos = empty_pos[idx];
            if(entity_grid[next_pos.i][next_pos.j].type == empty){
                herbi.energy -= MOVE_ENERGY;
                entity_grid[next_pos.i][next_pos.j] = {herbi.type, herbi.energy, herbi.age};
                entity_grid[cur_pos.i][cur_pos.j] = {empty, 0, 0};
                cur_pos = next_pos;

            }
        }

       entity_grid[cur_pos.i][cur_pos.j].age++;
}


void carni_routine(pos_t pos){
    pos_t& cur_pos = pos;
    entity_t carni;
    std::unique_lock lk(exec_it_mtx);
    while(!start) {
        exec_it.wait(lk);
    }
    carni = entity_grid[cur_pos.i][cur_pos.j];
    if(carni.type == empty or carni.energy == 0 or carni.age >= CARNIVORE_MAXIMUM_AGE) {  
            entity_grid[cur_pos.i][cur_pos.j] = {empty, 0, 0};
            return;
    }
    std::vector<pos_t> empty_pos;
    for(const pos_t& pos_to_verify : {pos_t(cur_pos.i, cur_pos.j+1), pos_t(cur_pos.i,cur_pos.j-1), pos_t(cur_pos.i+1,cur_pos.j), pos_t(cur_pos.i-1,cur_pos.j)}) {

            if(!(pos_to_verify.i >= 0 and pos_to_verify.i < NUM_ROWS and pos_to_verify.j >= 0 and pos_to_verify.j < NUM_ROWS)) {
				continue;
            }

            if(entity_grid[pos_to_verify.i][pos_to_verify.j].type == herbivore) {
                entity_grid[pos_to_verify.i][pos_to_verify.j] = {empty, 0, 0};
                carni.energy += CARNIVORE_ENERGY_GAIN;
            }
            
            if(entity_grid[pos_to_verify.i][pos_to_verify.j].type == empty)
                empty_pos.push_back(pos_to_verify);
        }

        if(mp_rand(gen) < CARNIVORE_REPRODUCTION_PROBABILITY and carni.energy > THRESHOLD_ENERGY_FOR_REPRODUCTION and !empty_pos.empty()) {
            size_t idx = mp_rand(gen) * empty_pos.size();
            std::vector<pos_t>::iterator idx_it = empty_pos.begin() + idx;
             pos_t child_pos = empty_pos[idx];
            if(entity_grid[child_pos.i][child_pos.j].type == empty){
                entity_grid[child_pos.i][child_pos.j] = {entity_type_t::carnivore, START_ENERGY, 0};
                //running_threads.push_back(std::thread(carni_routine, child_pos));
                empty_pos.erase(idx_it);
                carni.energy -= REPRODUCTION_ENERGY;
        }
        }

        if(mp_rand(gen) < CARNIVORE_MOVE_PROBABILITY and !empty_pos.empty()) {
            size_t idx = mp_rand(gen) * empty_pos.size();
            pos_t next_pos = empty_pos[idx];
            if(entity_grid[next_pos.i][next_pos.j].type == empty){
                carni.energy -= MOVE_ENERGY;
                entity_grid[next_pos.i][next_pos.j] = {carni.type, carni.energy, carni.age};
                entity_grid[cur_pos.i][cur_pos.j] = {empty, 0, 0};
                cur_pos = next_pos;

            }
        }
        entity_grid[cur_pos.i][cur_pos.j].age++;

}

void re_create_threads() {
    start=false;
    // Criar novas threads com base no estado atual da matriz
    for (size_t i = 0; i < entity_grid.size(); ++i) {
        for (size_t j = 0; j < entity_grid[i].size(); ++j) {
            entity_t entity = entity_grid[i][j];
            if (entity.type == plant) {
                running_threads.push_back(std::thread(plant_routine, pos_t(i, j)));
            } else if (entity.type == herbivore) {
                running_threads.push_back(std::thread(herbi_routine, pos_t(i, j)));
            } else if (entity.type == carnivore) {
                running_threads.push_back(std::thread(carni_routine, pos_t(i, j)));
            }
        }
    }
}
int main()
{
    crow::SimpleApp app;

    // Endpoint to serve the HTML page
    CROW_ROUTE(app, "/")
    ([](crow::request &, crow::response &res)
     {
        // Return the HTML content here
        res.set_static_file_info_unsafe("../public/index.html");
        res.end(); });

    CROW_ROUTE(app, "/start-simulation")
        .methods("POST"_method)([](crow::request &req, crow::response &res)
                                { 
        // Parse the JSON request body
        nlohmann::json request_body = nlohmann::json::parse(req.body);

       // Validate the request body
        uint32_t num_plant = request_body["plants"],
                 num_herbi = request_body["herbivores"],
                 num_carni = request_body["carnivores"];

        uint32_t total_entinties = num_plant + num_herbi + num_carni;
        if (total_entinties > NUM_ROWS * NUM_ROWS) {
        res.code = 400;
        res.body = "Too many entities";
        res.end();
        return;
        }

        // Clear the entity grid
        entity_grid.clear();
        entity_grid.assign(NUM_ROWS, std::vector<entity_t>(NUM_ROWS, { empty, 0, 0}));
        // Create the entities
        pos_t creation_pos;
        current_it = 0;
        start = false;
        for(int idx = 0; idx < num_plant; idx++) {
            creation_pos.i = rand_pos(gen);
            creation_pos.j = rand_pos(gen);
            entity_grid[creation_pos.i][creation_pos.j] = {plant, START_ENERGY, 0};
            running_threads.push_back(std::thread(plant_routine, creation_pos));
            
        }
        for(size_t idx = 0; idx != num_herbi; idx ++) {
            creation_pos.i = rand_pos(gen);
            creation_pos.j = rand_pos(gen);
            entity_grid[creation_pos.i][creation_pos.j] = {herbivore, START_ENERGY, 0};
            running_threads.push_back(std::thread(herbi_routine, creation_pos));
        }
        for(size_t idx = 0; idx != num_carni; idx ++) {
            creation_pos.i = rand_pos(gen);
            creation_pos.j = rand_pos(gen);
            entity_grid[creation_pos.i][creation_pos.j] = {carnivore, 100, 0};
            running_threads.push_back( std::thread(carni_routine, creation_pos));
        }
        // Return the JSON representation of the entity grid
        nlohmann::json json_grid = entity_grid; 
        res.body = json_grid.dump();
        res.end(); });

    // Endpoint to process HTTP GET requests for the next simulation iteration
 CROW_ROUTE(app, "/next-iteration")
        .methods("GET"_method)([]()
        {
        if(running_threads.empty()){
            re_create_threads();
        }
        start = true;
        exec_it.notify_all();
        for (int i=0; i < running_threads.size(); ++i){
            running_threads[i].join();
  
        }
        running_threads.clear();

        // Return the JSON representation of the entity grid
        nlohmann::json json_grid = entity_grid; 
        return json_grid.dump(); });
    app.port(8080).run();

    return 0;
}
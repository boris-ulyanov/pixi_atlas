cmake_minimum_required(VERSION 3.8)

project(example_pixi_atlas)

add_executable(${PROJECT_NAME} ./example.c ../pixi_atlas.c)

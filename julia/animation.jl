using Distributed
@everywhere using Pkg
@everywhere Pkg.activate(@__DIR__,io=devnull)
@everywhere include("solution.jl")

init_fun = gun
m = 12*5
n = 12*5
M = 2
N = 3
nodes = 1
steps = 300
worldstep = 1
game_serial(init_fun,m,n,steps,worldstep)
game_parallel(init_fun,m,n,M,N,nodes,steps,worldstep)

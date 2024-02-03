using Distributed
@everywhere using Pkg
@everywhere Pkg.activate(@__DIR__,io=devnull)

using Test

@everywhere include("solution.jl")
nodes = 1

@testset "Game tests" begin

    init_fun = glider
    m=12*2
    n=12*5
    M=1
    N=1
    steps=300
    worldstep=1
    @test game_check(init_fun,m,n,M,N,nodes,steps,worldstep)

    init_fun = beacon
    m=12*5
    n=12*4
    M=1
    N=1
    steps=300
    worldstep=1
    @test game_check(init_fun,m,n,M,N,nodes,steps,worldstep)

    init_fun = block
    m=12*5
    n=12*4
    M=1
    N=1
    steps=300
    worldstep=1
    @test game_check(init_fun,m,n,M,N,nodes,steps,worldstep)

    init_fun = gun
    m=50
    n=50
    M=1
    N=1
    steps=40
    worldstep=1
    @test game_check(init_fun,m,n,M,N,nodes,steps,worldstep)

    init_fun = glider
    m=12*5
    n=12*5
    M=3
    N=2
    steps=300
    worldstep=1
    @test game_check(init_fun,m,n,M,N,nodes,steps,worldstep)

    init_fun = beacon
    m=12*5
    n=12*5
    M=2
    N=3
    steps=300
    worldstep=1
    @test game_check(init_fun,m,n,M,N,nodes,steps,worldstep)

    init_fun = block
    m=12*5
    n=12*5
    M=2
    N=3
    steps=300
    worldstep=1
    @test game_check(init_fun,m,n,M,N,nodes,steps,worldstep)

    init_fun = gun
    m=12*5
    n=12*5
    M=3
    N=2
    steps=300
    worldstep=1
    @test game_check(init_fun,m,n,M,N,nodes,steps,worldstep)

    init_fun = glider
    m=12*5
    n=12*5
    M=3
    N=1
    steps=300
    worldstep=1
    @test game_check(init_fun,m,n,M,N,nodes,steps,worldstep)

end

nothing







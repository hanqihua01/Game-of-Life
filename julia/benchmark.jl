using Distributed
using ClusterManagers
ENV["JULIA_WORKER_TIMEOUT"] = 300

steps=10
worldstep = 0
nruns = 4

if length(ARGS) == 5
    m, n, M, N, nodes = map(i->parse(Int,i),ARGS)
    np = N*M
    addprocs(SlurmManager(np);t="00:15:00",N=nodes, exclusive="")
    @everywhere using Pkg
    @everywhere Pkg.activate(@__DIR__,io=devnull)
    @everywhere include("solution.jl")
    @info "Starting with case m=$m n=$n M=$M N=$N nodes=$nodes"
    init_fun = gun
    for irun in 1:nruns
        game_parallel(init_fun,m,n,M,N,nodes,steps,worldstep,irun)
    end
    @info    "Done with case m=$m n=$n M=$M N=$N nodes=$nodes"
elseif length(ARGS) == 2
    m, n, = map(i->parse(Int,i),ARGS)
    np = 1
    addprocs(SlurmManager(np);t="00:15:00",exclusive="")
    @everywhere using Pkg
    @everywhere Pkg.activate(@__DIR__,io=devnull)
    @everywhere include("solution.jl")
    w = minimum(workers())
    @info "Starting with case m=$m n=$n"
    msg = @fetchfrom w begin
        init_fun = gun
        for irun in 1:nruns
            game_serial(init_fun,m,n,steps,worldstep,irun)
        end
        "Done with case m=$m n=$n"
    end
    @info msg
else
    msg = """
    Usage

    julia --project=. benchmark.jl m n M N nodes

    or

    julia --project=. benchmark.jl m n
    """
    error(msg)
end

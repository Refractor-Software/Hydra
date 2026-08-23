# Developer Modules/Programs
I have no clue if these will be actual tools or not, but the point stands that they will live here.

Idea is that Hydra Studio (Engine.DevEnv) will want to do as much of its work through these developer modules/programs as possible, rather than having the relevant
workflows *only* available inside of it. That way we can run various things from the command line, either when Studio is in development (or broken and can't start),
or inside CI/CD workflows where it makes more sense to run a bunch of small programs than to try running Studio in some headless mode that it probably won't easily
support (especially if it ends up being a C#/WPF program like I expect).

Speaking of C#/WPF, that's another good reason to keep the heavy logic here - if we need to rewrite Studio later for new platforms (or to do one cross-platform build),
we don't have to excavate all of it from Studio. We will likely have a cross-platform UI framework to handle this later (RefractorSoftware.GUI if I had to guess at what I'll
name it), but right now C#/WPF is the fastest way to get something professional-looking up and running.

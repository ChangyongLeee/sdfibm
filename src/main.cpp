#include "fvCFD.H"
#include "solidcloud.h"

int main(int argc, char *argv[])
{
    #include "setRootCase.H"
    #include "createTime.H"
    #include "createMesh.H"
    #include "createFields.h"
    #include "initContinuityErrs.H"

    std::string dictfile;

    // if start-time > 0, read from start-time-folder for solidDict, otherwise read from case root
    if(runTime.time().value() > 0)
    {
        if(!Foam::Pstream::parRun())
            dictfile = "./" + mesh.time().timeName() + "/solidDict";
        else
            dictfile = "./processor0/" + mesh.time().timeName() + "/solidDict";
    }
    else
    {
        dictfile = "solidDict";
    }

    sdfibm::SolidCloud solidcloud(dictfile, U);

    while (runTime.loop())
    {
        Foam::Info << "Time = " << runTime.timeName() << Foam::endl;

        #include "CourantNo.H"
        Foam::dimensionedScalar dt = runTime.deltaT();

        if(solidcloud.isOnFluid())
        {
            Foam::fvVectorMatrix UEqn(
                fvm::ddt(U)
              + 1.5*fvc::div(phi, U) - 0.5*fvc::div(phi.oldTime(), U.oldTime())
              ==0.5*fvm::laplacian(nu, U) + 0.5*fvc::laplacian(nu, U));
            UEqn.solve();

            Foam::surfaceScalarField phiI("phiI", linearInterpolate(U) & mesh.Sf());
            Foam::fvScalarMatrix pEqn(fvm::laplacian(p) == fvc::div(phiI)/dt - fvc::div(Fs));
            pEqn.solve();

            U   = U    - dt*fvc::grad(p);
            phi = phiI - dt*fvc::snGrad(p)*mesh.magSf();
        }

        solidcloud.interact(runTime.value(), dt.value());

        if(solidcloud.isOnFluid())
        {
            U = U - Fs*dt;
            phi = phi - dt*(linearInterpolate(Fs) & mesh.Sf());

            U.correctBoundaryConditions();
            adjustPhi(phi, U, p);
            
            T = (1.0 - As)*T + Ts;
            T.correctBoundaryConditions();
            Foam::fvScalarMatrix TEqn(
                fvm::ddt(T)
              + fvm::div(phi, T)
              ==fvm::laplacian(alpha, T));
            TEqn.solve();

            #include "continuityErrs.H"
        }

        solidcloud.evolve(runTime.value(), dt.value());

        if(solidcloud.isOnFluid())
        {
            solidcloud.fixInternal(dt.value());
        }

        if(runTime.outputTime())
        {
            runTime.write();

            if(Foam::Pstream::master())
            {
                std::string file_name;
                if(Foam::Pstream::parRun())
                    file_name = "./processor0/" + runTime.timeName() + "/solidDict";
                else
                    file_name = "./" + runTime.timeName() + "/solidDict";
                solidcloud.saveRestart(file_name);
            }
        }
    }

    Foam::Info << "DONE\n" << endl;
    return 0;
}

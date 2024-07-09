#include "task2.h"
#include "sortEnergy.h"
#pragma once
#include <iostream>
#include <fstream>
#include <cmath> // Sau alte biblioteci necesare pentru calcule matematice
#include "TFile.h"
#include "TH2.h"
#include "TH1.h"
#include "TF1.h"
#include "TCanvas.h"

void fitGaussian(TF1 *gaus, TH1D *hist, double peakX, double rangeWidth)
{
    hist->Fit(gaus, "Q", "", peakX - rangeWidth, peakX + rangeWidth);
}

float extractBackgroundAreaForGaussian(TH1D *hist, int peakBin, TF1 *gaussian)
{
    float backgroundArea = 0;

    float sd = gaussian->GetParameter(2);

    int backgroundAreaWidth = (2 * sd); //The width of the adjustment range for the background

    int backgroundAreaPosition = peakBin - 20 - backgroundAreaWidth / 2;

    TF1 *gaussianBackground = new TF1("gaussianBackground", "pol1", backgroundAreaPosition, backgroundAreaPosition + backgroundAreaWidth);

    hist->Fit(gaussianBackground, "QN0", "", backgroundAreaPosition, backgroundAreaPosition + backgroundAreaWidth);

    backgroundArea = gaussianBackground->Integral(backgroundAreaPosition, backgroundAreaPosition + backgroundAreaWidth);

    // cout << "backgroundArea: " << backgroundArea << std::endl;

    delete gaussianBackground;

    return backgroundArea;
}

float calculateAreaOfGaussian(TF1 *gaussian)
{
    float amplitude = gaussian->GetParameter(0);
    float mean = gaussian->GetParameter(1);
    float sd = gaussian->GetParameter(2);

    float area = amplitude * sd * sqrt(2 * TMath::Pi());
    return area;
}

float areaPeak(TF1 *gaussian, TH1D *hist, int peakBin)
{
    float peakArea = calculateAreaOfGaussian(gaussian);
    float backgroundArea = extractBackgroundAreaForGaussian(hist, peakBin, gaussian);
    float area = peakArea - backgroundArea;
    //cout << "area: " << area << endl;
    return area;
}

float calculateResolution(TF1 *gaussian)
{
    float sigma = gaussian->GetParameter(2);
    float mean = gaussian->GetParameter(1);
    float resolution = sigma / mean;
    return resolution;
}

void eliminatePeak(TH1D *hist, int maxBin)
{
    for (int i = maxBin - 10; i <= maxBin + 10; i++)
    {
        hist->SetBinContent(i, 0);
    }
}
bool findStartOfPeak(TH1D *hist, int maxBin, TF1 *gaussian, int &leftLimitPosition, int &rightLimitPosition) {
    // Calculăm deviația standard a Gaussienei și FWHM
    float sigma = gaussian->GetParameter(2);
    double FWHM = 2.355 * sigma;

    // Setăm un factor pentru a determina intervalul pe care să îl explorăm în stânga și dreapta
    float sigmaFactor = 2.5; // Poți ajusta acest factor în funcție de specificul datelor tale
    
    // Determinăm limitele pentru căutarea pe partea stângă și pe partea dreaptă a vârfului
    int leftLimit = maxBin - static_cast<int>(FWHM * sigmaFactor);
    int rightLimit = maxBin + static_cast<int>(FWHM * sigmaFactor);
    
    // Verificăm limitele să fie în intervalul valabil al histogramei
    leftLimit = std::max(leftLimit, 1);
    rightLimit = std::min(rightLimit, hist->GetNbinsX());

    // Calculăm media ponderată a înălțimii în vecinătatea maximului local
    double meanHeight = 0;
    double sumHeight = 0;
    for (int i = maxBin - sigma; i <= maxBin + sigma; ++i) {
        double binContent = hist->GetBinContent(i);
        meanHeight += i * binContent;
        sumHeight += binContent;
    }
    meanHeight /= sumHeight;

    bool foundLeft = false;
    bool foundRight = false;

    // Căutăm punctul unde se atinge o anumită valoare relativ mică (de ex. 0.05 din media înălțimii) în stânga
    for (int i = maxBin - 1; i >= leftLimit; --i) {
        if ((hist->GetBinContent(i) + hist->GetBinContent(i + 1)) / 2 < 0.5 * meanHeight) {
            leftLimitPosition = i + 1; // Incrementăm pentru a avea exact punctul de început al peak-ului
            foundLeft = true;
            break;
        }
    }

    // Dacă nu găsim limita în stânga, folosim Gaussiană pentru a prezice poziția
    if (!foundLeft) {
        leftLimitPosition = maxBin - static_cast<int>(FWHM * sigmaFactor);
    }

    // Căutăm punctul unde se atinge o anumită valoare relativ mică (de ex. 0.5 din media înălțimii) în dreapta
    for (int i = maxBin + 1; i <= rightLimit; ++i) {
        if ((hist->GetBinContent(i) + hist->GetBinContent(i - 1)) / 2 < 0.5 * meanHeight) {
            rightLimitPosition = i - 1; // Decrementăm pentru a avea exact punctul de început al peak-ului
            foundRight = true;
            break;
        }
    }

    // Dacă nu găsim limita în dreapta, folosim Gaussiană pentru a prezice poziția
    if (!foundRight) {
        rightLimitPosition = maxBin + static_cast<int>(FWHM * sigmaFactor);
    }

    // Returnăm true dacă ambele limite au fost găsite sau prezise
    return foundLeft && foundRight;
}




int findPeak(TH1D *hist, int numBins, TH1D *mainHist, int peak, TF1 *gaus[])
{
    float maxPeakY = 0;
    int maxBin = 0;
    for (int bin = 0; bin <= numBins; ++bin) {
        float binContent = hist->GetBinContent(bin);
        if (binContent > maxPeakY) {
            maxPeakY = binContent;
            maxBin = bin;
        }
    }

    float maxPeakX = mainHist->GetXaxis()->GetBinCenter(maxBin);
    gaus[peak] = new TF1(Form("gaus%d_peak%d", peak, maxBin), "gaus", maxPeakX - 14, maxPeakX + 14); // Nume unic pentru fiecare fit Gaussian
    gaus[peak]->SetParameters(maxPeakY, maxPeakX, 5);
    mainHist->Fit(gaus[peak], "RQ+"); // Ajustare și desenare fit Gaussian pe histogramă
    int leftLimit, rightLimit;
    findStartOfPeak(mainHist, maxBin, gaus[peak], leftLimit, rightLimit);
    cout<<" LeftStart: "<<leftLimit<<" "<<"RightStart: "<<rightLimit<<endl;
    eliminatePeak(hist, maxBin);
    return maxBin;
}

void pritnInFileJson(ofstream &file, int column, int peak, int peak_position, float area, float resolution)
{
    file << "{" << endl;
    file << "  \"Column\": " << column << "," << endl;
    file << "  \"Peak\": " << peak << "," << endl;
    file << "  \"Position\": " << peak_position << "," << endl;
    file << "  \"Area\": " << area << "," << endl;
    file << "  \"Resolution\": " << resolution << endl;
    file << "}" << endl;
}


void sortTxt(double *&energyArray, int& size)
{
    ifstream inputFile("energy.txt");
    ofstream outputFile("output.txt");
    getEnergyArray(inputFile, energyArray, size);
    sortEnergy(energyArray, size);
    printToFile(outputFile, energyArray, size);
}
/*void calculateRegres(double x, int y, double& intercept, double& slope) {
    cout<<"x: "<<x<<" y: "<<y<<endl;
    TGraph *graph = new TGraph(size);
        graph->SetPoint(x, y);

    // Crearea unui obiect TF1 pentru regresia liniară
    TF1 *linearFit = new TF1("linearFit", "pol1"); // Polinom de grad 1 pentru regresia liniară

    // Aplicarea regresiei liniare pe grafic
    graph->Fit(linearFit, "Q"); // "Q" pentru fit rapid, fără afișare grafică

    // Extrage coeficienții dreptei de regresie liniară
    intercept = linearFit->GetParameter(0);
    slope = linearFit->GetParameter(1);

    // Eliberare memorie
    delete graph;
    delete linearFit;
    
}


// Funcție pentru verificarea corectitudinii regresiei liniare
bool checkRegres(double intercept, double slope, int* peaks, double* energyArray, int size) {
    bool isCorrectlyCorrelated = true;

    for (int i = 0; i < size; ++i) {
        double yCalc = slope * energyArray[i] + intercept;

        // Verifică corectitudinea asociatiei, poți ajusta toleranța aici
        if (fabs(peaks[i] - yCalc) > 1.0) {
            isCorrectlyCorrelated = false;
            break;
        }
    }

    return isCorrectlyCorrelated;
}

// Funcție pentru calibrare
void calibration(int* peaks, int size, double* energyArray) {
    double intercept = 0;
    double slope = 0;

    // Iterăm prin vârfuri și energii pentru a calibra
    for (int i = 0; i < size; ++i) {
        int x = peaks[i];
        cout<<"x: "<<x<<endl;
        // Încercăm mai multe valori de energie (x) pentru a găsi o regresie liniară potrivită
        for (int j = 0; j < size; ++j) {
            double y = energyArray[j];
            
            // Calculăm regresia liniară
            calculateRegres(x, y, intercept, slope);//problema memorie aici 

            // Verificăm corectitudinea regresiei
            //if (checkRegres(intercept, slope, peaks, energyArray, size)) {
                //std::cout << "Calibrated energy for peak " << y << " is " << x << std::endl;
                //break; // Ieșim din bucla internă dacă am găsit o corelație bună
            }
        }
    }
//}*/
double calculateError(double measured[], double known[], int size) {
    double error = 0.0;
    for (int i = 0; i < size; ++i) {
        error += fabs(measured[i] - known[i]);
    }
    return error;
}


// Verifică dacă energia prezisă se află în apropiere de una dintre energiile cunoscute
// Verifică dacă energia prezisă se află în apropiere de una dintre energiile cunoscute
bool checkPredictedEnergies(double predictedEnergy, double knownEnergies[], int size) {
    double minError = std::numeric_limits<double>::max();
    
    for (int i = 0; i < size; ++i) {
        if (fabs(predictedEnergy - knownEnergies[i]) < minError) {
            minError = fabs(predictedEnergy - knownEnergies[i]);
            //cout<<"energyKnown"<<knownEnergies[i]<<endl;
            //cout<<"predictedEnergy"<<predictedEnergy<<endl; 
            //cout<<"minError"<<minError<<endl;
        }
    }
    
    return minError < 10.0;
}
void outputDiferences(double knownEnergies[], int size, int peaks[], int peakCount) {
    for (int i = 0; i < peakCount; ++i) {
        for (int j = 0; j < size; ++j) {
            cout << "Diferența dintre " << knownEnergies[j]<<endl; //<< " și " << peaks[i] << " este " << fabs(knownEnergies[j] - peaks[i]) << std::endl;
        }
    }
    cout<<"---------------------------------"<<endl;
}
void calibratePeaks(int peaks[], int peakCount, double knownEnergies[], int size) {
    double bestM = 0.0;
    double bestB = 0.0;
    int bestCorrelation = 0;
    //outputDiferences(knownEnergies, size, peaks, peakCount);
    // Iterăm prin fiecare valoare posibilă pentru panta (m)
    for (double m = 0.1; m <= 10.0; m += 0.01) {
        for (double n = 0.0; n <= 5.0; n += 0.1) {
            int correlations = 0;
            
            // Încercăm diferite valori pentru fiecare peak
            for (int i = 0; i < peakCount; ++i) {
                // Calculăm energia prezisă pentru panta curentă (m) și intercept-ul (n) curent
                double predictedEnergy = m * peaks[i] + n;

                // Verificăm dacă energia prezisă se potrivește cu una dintre energiile cunoscute
                if (checkPredictedEnergies(predictedEnergy, knownEnergies, size)) {
                    ++correlations;
                }
            }

            // Actualizăm cea mai bună corelație găsită până acum
            if (correlations > bestCorrelation) {
                bestCorrelation = correlations;
                bestM = m; // Actualizăm panta pentru cea mai bună corelație
                bestB = n; // Actualizăm intercept-ul pentru cea mai bună corelație
            }

            // Dacă am găsit deja toate corelațiile posibile, întrerupem bucla
            if (bestCorrelation == peakCount) {
                break;
            }
        }

        // Dacă am găsit deja toate corelațiile posibile, întrerupem bucla
        if (bestCorrelation == peakCount) {
            break;
        }
    }

    // Afișăm rezultatele
    //std::cout << "Best m: " << bestM << ", Best b: " << bestB << ", Best Correlation: " << bestCorrelation << std::endl;
}
#include <TH1D.h>
#include <TF1.h>





void task1(int number_of_peaks, const char *file_path, double* energyArray, int size) {
    ofstream file("data.json");
    TFile *fr = new TFile(file_path, "READ");
    TFile *outputFile = new TFile("output.root", "RECREATE"); // Fișierul .root în care vom salva histogramele

    TH2F *h1;   
    fr->GetObject("mDelila_raw", h1);
    int number_of_columns = h1->GetNbinsX();

    for (int column = 1; column <= number_of_columns; column++) {
        TH1D *hist1D = h1->ProjectionY(Form("hist1D_col%d", column), column, column); // Nume unic pentru fiecare histogramă
        if (hist1D->GetMean() < 5) {
            delete hist1D;
            continue;
        }

        TCanvas *colCanvas = new TCanvas(Form("canvas_col%d", column), Form("canvas_col%d", column), 800, 600);

        int peaks[number_of_peaks];
        TF1 *gaus[number_of_peaks];
        TH1D *tempHist = (TH1D *)hist1D->Clone();
        for (int peak = 0; peak < number_of_peaks; peak++) {
            int peak_position = findPeak(tempHist, hist1D->GetNbinsX(), hist1D, peak, gaus);
            peaks[peak] = peak_position;
            hist1D->Fit(gaus[peak], "RQ+"); // Ajustare și desenare fit Gaussian pe histogramă
            //fitGaussian(gaus[peak], hist1D, peak_position, 14);
            //fitGaussian(gaus[peak], hist1D, peak_position, 14);
        }


        // Afișează histograma pe canvasul dedicat coloanei
        colCanvas->cd();
        hist1D->Draw();
        colCanvas->Update();

        // Salvează canvasul în fișierul ROOT
        colCanvas->Write();

        // Eliberăm memoria alocată pentru canvas și histograme
        delete hist1D;
        delete colCanvas;

        // Calibrare vârfuri
        calibratePeaks(peaks, number_of_peaks, energyArray, size);
    }

    // Închidem fișierele .root și .json
    outputFile->Close();
    fr->Close();
    file.close();

    // Eliberăm memoria alocată pentru TFile-uri
    delete outputFile;
    delete fr;
}


int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <number_of_peaks> <file_path>" << std::endl;
        return 1;
    }
    int number_of_peaks = std::atoi(argv[1]);
    const char *file_path = argv[2];
    double *energyArray = nullptr;
    int size;
    sortTxt(energyArray, size);

    double calibTest[10] = { 69.8, 134.8, 199.8, 264.8, 329.8, 394.8, 459.8, 524.8, 589.8, 654.8 };
    task1(number_of_peaks, file_path, energyArray, size);
    //task1(number_of_peaks, file_path, calibTest, 10);
    //calibration(peaks, size, energyArray);
    
    delete[] energyArray;
    return 0;
}

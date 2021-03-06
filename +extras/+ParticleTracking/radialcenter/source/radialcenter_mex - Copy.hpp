/*--------------------------------------------------
Copyright 2018-2019, Daniel T. Kovari, Emory University
All rights reserved.
----------------------------------------------------*/
#pragma once

#include <mex.h>
#include <extras/string_extras.hpp>
#include "radialcenter.hpp"

#include <extras/Array.hpp>
#include <extras/cmex/NumericArray.hpp>
#include <extras/cmex/mxparamparse.hpp>

namespace extras{namespace ParticleTracking{

	//! Convert string into valid COMmethod
	//! throws error if string does not correspond to valid method
	//!
	//! Valid Strings:
	//!		"meanabs"
	//!		"normal"
	//!		"gradmag"
	rcdefs::COM_METHOD string2COMmethod(std::string COMmeth) {
		//validate COMmethod
		COMmeth = tolower(COMmeth);

		if (COMmeth.compare("meanabs") == 0) {
			return rcdefs::MEAN_ABS;
		}
		else if (COMmeth.compare("normal") == 0) {
			return rcdefs::NORMAL;
		}
		else if (COMmeth.compare("gradmag") == 0) {
			return rcdefs::GRAD_MAG;
		}
		else {
			throw(std::runtime_error("COMmethod invalid"));
		}
	}

    template<class OutContainerClass> //C must be and ArrayBase derived class
    std::vector<OutContainerClass> radialcenter(const mxArray* pI,
                                const RadialcenterParameters_Shared& params = RadialcenterParameters_Shared())
    {
        switch (mxGetClassID(pI)) { //handle different image types seperatelys
    	case mxDOUBLE_CLASS:
    		return radialcenter<OutContainerClass,double>(cmex::NumericArray<double>(pI), params);
    	case mxSINGLE_CLASS:
    		return radialcenter<OutContainerClass,float_t>(cmex::NumericArray<float>(pI), params);
    	case mxINT8_CLASS:
    		return radialcenter<OutContainerClass,int8_t>(cmex::NumericArray<int8_t>(pI), params);
    	case mxUINT8_CLASS:
    		return radialcenter<OutContainerClass,uint8_t>(cmex::NumericArray<uint8_t>(pI), params);
    	case mxINT16_CLASS:
    		return radialcenter<OutContainerClass,int16_t>(cmex::NumericArray<int16_t>(pI), params);
    	case mxUINT16_CLASS:
    		return radialcenter<OutContainerClass,uint16_t>(cmex::NumericArray<uint16_t>(pI), params);
    	case mxINT32_CLASS:
    		return radialcenter<OutContainerClass,int32_t>(cmex::NumericArray<int32_t>(pI), params);
    	case mxUINT32_CLASS:
    		return radialcenter<OutContainerClass,uint32_t>(cmex::NumericArray<uint32_t>(pI), params);
    	case mxINT64_CLASS:
    		return radialcenter<OutContainerClass,int64_t>(cmex::NumericArray<int64_t>(pI), params);
    	case mxUINT64_CLASS:
    		return radialcenter<OutContainerClass,uint64_t>(cmex::NumericArray<uint64_t>(pI), params);
    	default:
    		throw(std::runtime_error("radialcenter: Only numeric image types allowed"));
    	}
    }

    /// Wrapper for radialcenter, accepting the standard arguments for a mexFunction
    /*
    % [x,y,varXY,d2] = radialcenter(I,WIND)
    %                = radialcenter(__,name,value);
    %
    % Estimate the center of radial symmetry of an image
    %
    % Input:
    %   I: the image to process
    %   WIND: [N x 4] specifying windows [x,y,w,h], default is entire image
    %
    % Output:
    %   x,y: center positions
    %
    %   varXY: variance estimate of the fit
    %       varXY = [Vx,Vy], where Vx and Vy are the variances of each X and Y
    %
    %   d2: the square of the weighted residual, normalized by the effective number of pixels
    %       d2>>1 indicates poor localization. This roughly characterizes the
    %       distance between each gradient line and the determined center
    %       location.
    %
    %      In practice, d2 is a good metric for determing if an image has an
    %      apparent symmetric center, while varXY is useful for characterizing the
    %      precision of the fit
    %
    %
    % Name,Value Parameters:
    % -------------------------
    %   'RadiusCutoff',val or [v1,v2,...vN]: fringe size cutoff
	%	'CutoffFactor',val or [v1,v2,...vN]: size cutoff is applied by wieghting using a logistic function :1/(1 + exp(CutoffFactor*(r_guess - RadiusCutoff)));
	%		where r_guess is the estimated center of symmetry (either supplied via XYC or found by Image central moment, aka image "center of mass")
	%		default = INFINITY (i.e. top-hat function)
    %   'XYc',[X,Y] : particle center estimates
    %   'COMmethod',method
    %       method='meanABS' : use COM on |I-mean(I)| to estimate center for radius filter
    %       method='normal': use COM on unmodified I to estimate center
    %       method='gradmag': use magnitude of image gradient to find COM (defalut)
    %   'DistanceExponent',value or [v1,v2,...,vN]: distance scaling from center guess Wii *= 1/r_guess^(DistanceExponent)
    %	'GradientExponent',value or [v1,v2,...,vN]: gradient scaling from center guess Wii *= |GradI_i|^(DistanceExponent)
    */
    void radialcenter_mex(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
    {
    	if (nrhs<1) { //not enough inputs
    		mexErrMsgIdAndTxt("MATLAB:radialcenter:invalidNumInputs",
    			"At least one input required.");
    	}
        if (nlhs<1){ //nothing to do.
            return;
        }

    	cmex::NumericArray<double> WIND;
		bool found_wind = false;

    	int ParamIndex = 1;

    	if (nrhs > 1 && !mxIsChar(prhs[1])) {
    		WIND = prhs[1];
    		ParamIndex = 2;

            if(!WIND.isempty()){
                if(WIND.nCols()!=4){
                    throw(std::runtime_error("WIND must be [n x 4]"));
                }
                for(size_t n=0;n<WIND.nRows();++n){ //fix 1-index --> 0-index
                    WIND(n,0)-= 1;
                    WIND(n,1)-=1;
                }
            }
			found_wind = true;
    	}

    	RadialcenterParameters_Shared params;
		cmex::MxInputParser Parser(false); //create non-case sensitive input parser
		Parser.AddParameter("RadiusCutoff",INFINITY); //default to no radius cutoff
		Parser.AddParameter("CutoffFactor", INFINITY); //default to top-hat function
		Parser.AddParameter("DistanceExponent", 1); //default to top-hat function
		Parser.AddParameter("GradientExponent", 5); //default to top-hat function
		Parser.AddParameter("XYc");
		Parser.AddParameter("COMmethod", "gradmag");
		if(!found_wind){
			Parser.AddParameter("Window");
		}

    	if (ParamIndex < nrhs) {
			/// Parse value pair inputs
    		int res = Parser.Parse(nrhs - ParamIndex, &prhs[ParamIndex]);
    		if (res != 0) {
    			throw(std::runtime_error("could not parse input parameters"));
    		}

    		params.RadiusCutoff = std::make_shared<cmex::NumericArray<double>>(Parser("RadiusCutoff"));
			params.CutoffFactor = std::make_shared<cmex::NumericArray<double>>(Parser("CutoffFactor"));
			params.DistanceExponent = std::make_shared<cmex::NumericArray<double>>(Parser("DistanceExponent"));
			params.GradientExponent = std::make_shared<cmex::NumericArray<double>>(Parser("GradientExponent"));
    		params.XYc = std::make_shared<cmex::NumericArray<double>>(Parser("XYc"));
    		//shift from 1-indexing
    		(*params.XYc.get())-=1;

			//validate COMmethod
			params.COMmethod = string2COMmethod(cmex::getstring(Parser("COMmethod")));
    	}

		if(found_wind){
			params.WIND = std::make_shared<cmex::NumericArray<double>>(WIND.getmxarray());
		}else{
			params.WIND = std::make_shared<cmex::NumericArray<double>>(Parser("Window"));
		}

    	//mexPrintf("About to run radial center...\n");
    	try {
    		auto out = radialcenter<cmex::NumericArray<double>>(prhs[0], params);

    		if (nlhs > 0) {
    			out[0]+=1;
    			plhs[0] = out[0];
    		}
    		if (nlhs > 1) {
    			out[1]+=1;
    			plhs[1] = out[1];
    		}
    		if (nlhs > 2) {
    			plhs[2] = out[2];
    		}
    		if (nlhs > 3) {
    			plhs[3] = out[3];
    		}
    	}
    	catch (std::exception& e) {
    		mexErrMsgTxt(e.what());
    	}
    }
}}

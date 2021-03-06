classdef roiManager3D < extras.roi.roiManager
% Extension of roiManager to work with roiObject3D and create proper
% context menu
%% Copyright 2019 Daniel T. Kovari, Emory University
%   All rights reserved.
    
    %% Internal Use - overloadable createROI static function
    methods (Static)
        function roi = CreateROI(varargin) %alias function for creating roi objects. NOTE: created rois are not added to the managed list
            roi = extras.roi.roiObject3D(varargin{:});
        end
    end
    
    %creator
    methods
        function this = roiManager3D()
            this.changeObjectClassName('extras.roi.roiObject3D');
        end
    end
    
    %context generator customization
    methods
        function cg = createContextGenerators(this,roiObjs)
            %redefinable method for creating extras.roi.ContextGenerator
            %objects from roiObjects
            cg = extras.roi.ContextGenerator3D(roiObjs,this);
        end
    end
end
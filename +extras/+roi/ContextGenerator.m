classdef ContextGenerator < handle & matlab.mixin.SetGet & matlab.mixin.Heterogeneous
% extras.roi.ContextGenerator Class for generating uicontextmenus linked to 
% ROI objects.

    properties (SetAccess=protected)
        RoiObject extras.roi.roiObject = extras.roi.roiObject.empty();
        ContextMenus = gobjects(0);
    end
    properties(Access=protected)
        RoiObjectDeleteListener
    end
    
    %% create/delete
    methods
        function this = ContextGenerator(RoiObject)
        % ContextGenerator(RoiObject)
        %
        % Input:
        %   RoiObject: class handle to roiObject which is linked to the
        %   context menu;
            %% Validate RoiObject
            if nargin<1
                delete(this);
                this = extras.roi.ContextGenerator.empty();
                return;
            end
            if isempty(RoiObject)
                delete(this);
                this = extras.roi.ContextGenerator.empty();
                return;
            end
            assert(isvalid(RoiObject)&&isa(RoiObject,'extras.roi.roiObject'),'RoiObject must be valid extras.roi.roiObject');
            
            %% handle array
            sz = num2cell(size(RoiObject));
            this(sz{:}) = this;
            for n=1:numel(RoiObject)
                this(n).RoiObject = RoiObject(n);
                this(n).RoiObjectDeleteListener = addlistener(RoiObject(n),'ObjectBeingDestroyed',@(~,~) delete(this(n)));
            end
        end
        
        function delete(this)
            for n=1:numel(this)
                try
                    delete(this(n).RoiObjectDeleteListener);
                catch
                end
                try
                    delete(this(n).ContextMenus);
                catch
                end
            end
        end
    end
    
    %% Public Methods (non-overridable)
    methods (Sealed)
        function createContextMenu(this,hFig)
            
            %% remove invalid figure handles
            hFig(~isgraphics(hFig)) = [];
            if isempty(hFig)
                return;
            end
            assert(all(strcmpi('figure',{hFig.Type})),'Specified parent handles must be figures');
            
            %% Loop over all this and all hFig
            for n=1:numel(this)
                for m=1:numel(hFig)
                    %% Create context menu
                    cm = this(n).internal_createContextMenu(hFig);
                    
                    %% add to list of contextmenus
                    this(n).ContextMenus = [this(n).ContextMenus,cm];
                end
            end
            
            
        end
    end
    
    %% internal use only
    methods(Access=protected)
        function clearInvalidContextMenus(this,hCM)
            for n=1:numel(this)
                if nargin>1
                    this(n).ContextMenus(this(n).ContextMenus == hCM) = [];
                end
                this(n).ContextMenus(~isvalid(this(n).ContextMenus)) = [];
            end
        end
    end
    
    %% Overloadable delete roiObject callback
    methods (Static,Access=protected)
        function deleteRoiCallback(hROI)
            % redefine this method to change what happens when a user
            % selects delete from context menu
            delete(hROI);
        end
    end
    methods(Access=protected)
        function cm = internal_createContextMenu(this,hFig)
        %redefine this method to change items that are included in the
        %context menu
            cm = uicontextmenu(hFig);
            addlistener(cm,'ObjectBeingDestroyed',@(h,~) this.clearInvalidContextMenus(h));

            % delete menu
            uimenu(cm,'Text','Delete ROI',...
                'Separator','on',...
                'ForegroundColor','r',...
                'MenuSelectedFcn',@(~,~) this.deleteRoiCallback(this.RoiObject));
        end
    end
end


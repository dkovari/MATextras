classdef MercuryHub < extras.hardware.SerialDevice
%% Object for communicating with PI Mercury Controllers via serial port
%
% The constructor for this class is protected, so you cannot create an
% instance directly.
% Instead, you should search for an existing Hub connected to a given
% serial port using findHub()
%
%   > hub = extras.hardware.PI.MercuryHub.findHub('COM5');
%       or
%   > hub = extras.hardware.PI.MercuryHub.findHub('Port','COM5');
%       
% If no hub is connected, a new hub will be created.
%
% You can create a new hub by not passing any arguments or passing an empty
% port
%   > newHub = extras.hardware.PI.MercuryHub.findHub()
%       or
%   > newHub = extras.hardware.PI.MercuryHub.findHub([])
%       or
%   > newHub = extras.hardware.PI.MercuryHub.findHub('Port',[])
%
% You can specify device parameters by including Name-Value pairs after the
% port. (Note, the port parameter must be listed first)
%   > newHub = extras.hardware.PI.MercuryHub.findHub('COM5',PropName,Value,...)
%       or
%   > newHub = extras.hardware.PI.MercuryHub.findHub([],PropName,Value,...)
%       or
%   > newHub = extras.hardware.PI.MercuryHub.findHub('Port','COM6',PropName,Value,...)
%
% *************************************************************************
% Developer Notes:
% *************************************************************************
% Automatic Hub deletion
% =============================
% if you no longer need a reference to a Hub object (returned by findHub)
% you should call:
% > DecrementReferenceCount(hubObj)
% This decrements the hub's internal reference counter.
% When the counter hits zero, the hub will be deleted.

    %% Static Property
    properties (Constant)
        HubList = extras.hardware.PI.HubList;
        ResponseTimeout = 0.4; %Ti
        CommandWaitPeriod = 0.04;
        ParseTimeout = 2;
    end
    
    %% Internal Properties
	properties (SetAccess=protected)
        DeviceMap = containers.Map.empty;
        BoardList = []; %list of detected boad IDs
    end
    properties (Access=protected)
        ReferenceCount = 0; %number of device objects connected to this hub
    end
    
    %% Protected Create method
    methods (Access=protected)
        function this = MercuryHub()
            
            %% Set Serial Properties
            this.BaudRate = 9600;
            this.DataBits = 8;
            this.StopBits = 1;
            this.Parity = 'none';
            this.Terminator = {3,'CR'};
            %this.BytesAvailableFcn = @(~,~) this.ProcessSerialBuffer;
            
            %% Setup device map
            this.DeviceMap = containers.Map('KeyType','uint32','ValueType','any');
            
            %% 
            this.HubList.add(this); %add this hub instance to the hub list
        end
    end
    
    %% delete
    methods
        function delete(this)
            %% Delete DeviceMap
            DL = values(this.DeviceMap);
            for n=1:numel(DL)
                try
                    delete(DL{n})
                catch
                end
            end
        end
    end
    
    %% Public findHub method
    methods (Static)
        function Hub = findHub(Port,varargin)
        % Find or create a Hub with associated port
            if nargin < 1
                Port = [];
            end

            if ~isempty(Port)&&ischar(Port)&&strcmpi(Port,'Port')
                assert(~isempty(varargin),'''Port'' option was specified in first argument, next argument should be a valid port name');
                Port = varargin{1};
                varargin(1)=[];
                
            end

            if isempty(Port)
                Hub = extras.hardware.PI.MercuryHub();
                set(Hub,varargin{:});
            else
                assert(ischar(Port),'Port must be a valid char array specifying com port name (e.g. ''COM5'')');

                %find the port
                Hmatch = findobj(extras.hardware.PI.MercuryHub.HubList(:),'Port',Port);

                if numel(Hmatch)>1
                    error('Multiple hubs with port: %s were returned...DAN, HELP!!!!',Port);
                end

                if isempty(Hmatch)
                    Hub = extras.hardware.PI.MercuryHub();
                    set(Hub,'Port',Port,varargin{:});
                    try
                        Hub.ConnectCOM();
                    catch ME %if connection fails, delete created object and throw error
                        delete(Hub);
                        rethrow(ME);
                    end
                else
                    Hub = Hmatch;
                end

            end
            
            Hub.ReferenceCount  = Hub.ReferenceCount+1;
        end
    end
    methods
        function DecrementReferenceCount(this)
            this.ReferenceCount = this.ReferenceCount-1;
            if this.ReferenceCount < 1
                delete(this);
            end
        end
    end
    
    %% Overload ConnectCOM to search HubList for other Hubs connected to this com
    methods
        function ConnectCOM(this,PORT)
            if nargin<2
                PORT = this.Port;
            end
            
            %% look for other hubs with this port
            hubs = findobj(extras.hardware.PI.MercuryHub.HubList(:),'Port',PORT);
            if numel(hubs)>1 || ~isempty(hubs)&&(hubs(1)~=this)
                error('A extras.hardware.PI.MercuryHub with Port: %s already exists. Cannont connect to that port. Use extras.hardware.PI.MercuryHub.findHub() to identify Hub object',PORT);
            end
            
            %% Connect
            ConnectCOM@extras.hardware.SerialDevice(this,PORT);
            
            %% Setup serial callback
            this.BytesAvailableFcn = @(~,~) this.ProcessSerialBuffer;
        end
    end

    %% overload validateConnection (Protected)
    methods (Access=protected)
        function validateConnection(this) %called after serial device connects, throw error if connection failed
            %this.scom.BytesAvailableFcn = '';
            this.BoardList = [];
            %% Scan for connected boards
            fprintf('Scanning for Mercury Controllers on %s\n',this.Port);
            for b = 0:15
                str=[1,dec2hex(b),'xx,TB'];
                fprintf(this.scom,str);
                t1=tic;
                while this.scom.BytesAvailable<=0
                    if toc(t1)>this.ResponseTimeout
                        break;
                    end
                end
                if this.scom.BytesAvailable>0
                    resp = fgetl(this.scom);
                    
                    %% Look for responses
                    respc = regexp(resp,'B:[\+\-]{0,1}\d+','match');
                    if isempty(respc)
                        fprintf('\tDid not find board: %d\n',b);
                        continue;
                    end
                    if numel(respc)>1
                        warning('multiple board responses were captured');
                    end
                    for n=1:numel(respc)
                        brd = sscanf(respc{n},'B:%d',1);
                        if numel(brd)~=1
                            warning('Could not interpret board response: %s',respc{n});
                            continue;
                        end
                        %% Found Board!!!
                        if brd==b
                            fprintf('\tFound board: %d\n',b);
                            this.BoardList = [this.BoardList,b];
                            break;
                        end
                    end
                else
                    fprintf('\tDid not find board: %d\n',b);
                end
            end
            
            if isempty(this.BoardList)
                this.connected = false;
                fprintf('Closing %s\n',this.Port);
                fclose(this.scom);
                error('Did not find any Mercury Contorllers on Port: %s',this.Port);
            end
            
            %% Reset BytesAvailableFcn
            %this.scom.BytesAvailableFcn = this.BytesAvailableFcn;
            
        end
    end
    
    %% User Accessible Functions
    methods
        function sendCommand(this,BoardID,cmd,varargin)
            if ~this.connected
                error('Cannont send command, serial connection has not been established');
            end
            assert(isscalar(BoardID)&&isnumeric(BoardID)&&BoardID>=0,'Invalid BoardID');
            assert(ischar(cmd),'cmd must be a char array');
            
            persistent LastSend; %timepoint of last serial fprintf
            if isempty(LastSend)
                LastSend = -Inf;
            end
            
            while (now-LastSend)*24*3600 < this.CommandWaitPeriod
                drawnow limitrate;
                pause(0.002);
            end
            fprintf(this.scom,[1,dec2hex(BoardID),'xx,TB,',cmd],varargin{:});
            
            %disp([1,dec2hex(BoardID),'xx,TB,',cmd])
            
            LastSend = now;
        end
    end
    
    %% Process Serial Buffer
    methods (Hidden)
        function ProcessSerialBuffer(this) %called whenever a terminator is read in the serial buffer
            persistent LastBoard; %fallback incase board could not be interpreted
            if isempty(LastBoard)
                LastBoard = 0;
            end
            
            if this.scom.BytesAvailable <1 %something already processed the next message, probably a previous instance of the loop
                return;
            end
            
            %% Determine BoardID
            resp = fgetl(this.scom);
            respc = regexp(resp,'B:[\+\-]{0,1}\d+','match');
            if ~isempty(respc)
                Board = sscanf(respc{end},'B:%d',1);
                assert(numel(Board)==1,'Error interpreting board id');
                LastBoard = Board;
                
                % grab the next message
                resp = fgetl(this.scom);
            end
            
            %% If Board is setup, process the message
            respc = regexp(resp,'[A-Z]:(([0-9A-F]{2}\s){5}[0-9A-F]{2}|[\+\-]{0,1}\d+)','match');
            if numel(respc)>1
                warning('Multiple commands recieved.');
            end
            try
            for n=1:numel(respc)
                if respc{n}(1)=='B'
                    Board = sscanf(respc{n},'B:%d',1);
                    assert(numel(Board)==1,'Error interpreting board id');
                    LastBoard = Board;
                    continue;
                end
                
                if this.DeviceMap.isKey(LastBoard) % only process data if the current board has been added to the DeviceMap
                    dev = this.DeviceMap(LastBoard); %get ref to device since matlab won't let us directly call methods on objects returned by objectmaps
                    
                    switch(respc{n}(1))
                        case 'A' %Tell Analog 
                        case 'H' %Tell channel (digital input)
                        case 'N' %dynamic target
                        case 'E' %tell error
                        case 'F' %profile following error
                        case 'X' %iteration number
                        case 'L' %programmed accelleration
                            VAL = sscanf(respc{n},'L:%d',1);
                            if numel(VAL)~=1
                                warning('Error interpreting Tell acceleration: %s',resp{n});
                            end
                            dev.updateAcceleration(VAL);
                        case 'M' %macro
                        case 'P' %position
                            VAL = sscanf(respc{n},'P:%d',1);
                            if numel(VAL)~=1
                                warning('Error interpreting Tell Position: %s',resp{n});
                            end
                                dev.updatePosition(VAL);
                        case 'S' %status
                            dev.updateStatus(respc{n}(3:end));
                        case 'T' %target
                            VAL = sscanf(respc{n},'T:%d',1);
                            if numel(VAL)~=1
                                warning('Error interpreting Tell Target: %s',resp{n});
                            end
                            dev.updateTarget(VAL);
                        case 'V' %actual velocity
                        case 'Y' %programmed velocity
                            VAL = sscanf(respc{n},'Y:%d',1);
                            if numel(VAL)~=1
                                warning('Error interpreting Tell Programmed Velocity: %s',resp{n});
                            end
                            dev.updateProgrammedVelocity(VAL);
                        otherwise
                            warning('Message: %s could not be interpreted',resp);
                            return;
                    end
                    
                end

            end
            catch ME
                disp(ME.getReport)
                rethrow(ME);
            end
            
        end
    end
end

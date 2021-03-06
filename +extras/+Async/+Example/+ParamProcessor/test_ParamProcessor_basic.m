I = rand(250);
WIND = rand(1,4);

%% get pointer
p = extras.Async.Example.ParamProcessor.ExampleParamProcessorMex('new');
extras.Async.Example.ParamProcessor.ExampleParamProcessorMex('pause',p);
extras.Async.Example.ParamProcessor.ExampleParamProcessorMex('saveResults',p,true);

'press a key 1'
pause
%% push data
extras.Async.Example.ParamProcessor.ExampleParamProcessorMex('setParameters',p,'Window',WIND);
extras.Async.Example.ParamProcessor.ExampleParamProcessorMex('pushTask',p,I);

'press a key 2'
pause();

PUSH_PROC = extras.Async.Example.ParamProcessor.ExampleParamProcessorMex('getPushedProcced',p)


remTasks = extras.Async.Example.ParamProcessor.ExampleParamProcessorMex('remainingTasks',p)

nRes = extras.Async.Example.ParamProcessor.ExampleParamProcessorMex('availableResults',p)

nArgOut = extras.Async.Example.ParamProcessor.ExampleParamProcessorMex('numResultOutputArgs',p)
out = cell(1,nArgOut);
[out{:}] =  extras.Async.Example.ParamProcessor.ExampleParamProcessorMex('popResult',p)


%% delete
'press key to delete'
pause
extras.Async.Example.ParamProcessor.ExampleParamProcessorMex('pause',p)
extras.Async.Example.ParamProcessor.ExampleParamProcessorMex('cancelRemainingTasks',p)
remTasks = extras.Async.Example.ParamProcessor.ExampleParamProcessorMex('remainingTasks',p)
extras.Async.Example.ParamProcessor.ExampleParamProcessorMex('delete',p)
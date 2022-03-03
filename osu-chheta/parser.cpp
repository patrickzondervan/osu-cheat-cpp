#include "parser.hpp"
#include "hitobject.hpp"
#include "beatmap.h"

Action::Action()
{
    this->time = -1;
    this->duration = -1;
    this->key = '\0';
    this->input = {};

    this->pressed = false;
}

Action::Action(HitObject* hitObject, char key, bool pressed)
{
    this->time = hitObject->getTime();
    this->duration = -1;

    if (hitObject->getType() == HitObjectType::Spinner)
    {
        this->duration = dynamic_cast<SpinnerHitObject*>(hitObject)->getEndTime() - hitObject->getTime();
    }

    if(!pressed)
    {
        if (hitObject->getType() == HitObjectType::Circle)
        {
            this->time += 4;
        } else
        {
            this->time += this->duration;
        }
    }

    this->key = key;
    this->setInput(pressed);
}

Action::Action(double sliderMultiplier, double beatLength, TimingPoint* timingPoint, HitObject* hitObject, char key, bool pressed)
{
    printf("%i, %i\n", this->duration, beatLength);

    SliderHitObject* sliderObject = (SliderHitObject*) hitObject;

    this->time = hitObject->getTime();
    this->duration = (sliderObject->getSliderLength() / (sliderMultiplier * 100) * (beatLength * (timingPoint->isInherited() ? 1.0 : std::abs(timingPoint->getBeatLength()) / 100))) * sliderObject->getSlides();


    if (!pressed)
    {
        this->time += this->duration;
    }

    this->key = key;
    this->setInput(pressed);
}

void Action::setInput(bool pressed)
{
    this->pressed = pressed;
    this->input = { };

    this->input.type = INPUT_KEYBOARD;
    this->input.ki.wVk = VkKeyScan(key);
    this->input.ki.dwFlags = (pressed ? 0 : KEYEVENTF_KEYUP);
}

std::vector<std::string> parseHitObject(std::string objectString)
{
    std::vector<std::string> vec;

    int delimeter = 0;

    while ((delimeter = objectString.find(",")) != std::string::npos)
    {
        vec.push_back(objectString.substr(0, delimeter));
        objectString.erase(0, delimeter + 1);
    }

    if (!objectString.empty())
    {
        vec.push_back(objectString);
    }

    return vec;
}

const std::map<std::string, int> sectionMap = {
    { "[TimingPoints]", 0 },
    { "[HitObjects]", 1 },
    { "[Difficulty]", 2 }
};

using funcPtr = void(Beatmap::*)(float);

std::map<std::string, funcPtr> beatmapInitializers = {
    { "ApproachRate:", &Beatmap::setApproachRate },
    { "OverallDifficulty:", &Beatmap::setOverallDifficulty },
    { "CircleSize:", &Beatmap::setCircleSize },
    { "SliderMultiplier:", &Beatmap::setSliderMultiplier },
};

double getCorrectBeatLength(std::vector<TimingPoint>* timingPoints, TimingPoint* currentTimingPoint, std::vector<HitObject> hitObjects)
{
    double beatLength = currentTimingPoint->getBeatLength();

    for (auto point : *timingPoints)
    {
        std::cout << point.getBeatLength() << std::endl;

        if (hitObjects[0].getTime() == currentTimingPoint->getTime())
        {
            break;
        }

        currentTimingPoint = &point;
    }

    return beatLength;
}

Beatmap parseMap(std::string path)
{
    Beatmap* beatmap = new Beatmap();

    std::ifstream file(path);
    std::vector<Action> actions;

    if (!file)
    {
        return *beatmap;
    }

    std::vector<HitObject> hitObjects;
    std::vector<TimingPoint> timingPoints;

    std::string line;

    int section = -1;
    double sliderMultiplier = 0;

    std::cout << "[parsing] parsing contents of" + path + "osu! beatmap" << std::endl;

    while (std::getline(file, line))
    {
        if (!strcmp(line.c_str(), ""))
        {
            section = -1;
        } else {
            bool shouldContinue = true;

            for (auto entry : sectionMap)
            {
                if (entry.first.compare(line) == 0)
                {
                    section = entry.second;
                    shouldContinue = false;
                    break;
                }
            }

            if (shouldContinue)
            {
                if (section == 0)
                {
                    timingPoints.push_back(TimingPoint(line));
                }
                else if (section == 1)
                {
                    hitObjects.push_back(createHitObject(line));
                }
                else if (section == 2)
                {
                    for (auto initializer : beatmapInitializers)
                    {
                        auto function = initializer.second;

                        if (line.find(initializer.first) == 0)
                        {
                            (beatmap->*function)(std::stod(line.substr(line.find(":") + 1)));
                        }

                        if (beatmap->isFilled())
                        {
                            section = -1;
                        }
                    }
                }
            }
        }
    }

    TimingPoint* currentTimingPoint = &*timingPoints.begin();

    std::cout << currentTimingPoint->getBeatLength() << std::endl;

    double beatLength = timingPoints.begin()->getBeatLength();
    char key = PRIMARY_KEY;

    int lastTime = 0;

    printf("[actions] creating actions from HitObjects");

    for (auto object : hitObjects)
    {
        beatLength = getCorrectBeatLength(&timingPoints, currentTimingPoint, hitObjects);

        if (currentTimingPoint->isInherited())
        {
            beatLength = currentTimingPoint->getBeatLength();
        }

        if (object.getType() == HitObjectType::Circle || object.getType() == HitObjectType::Spinner)
        {
            actions.push_back(Action(&object, key, true));
            actions.push_back(Action(&object, key, false));
        } else if (object.getType() == HitObjectType::Slider)
        {
            actions.push_back(Action(sliderMultiplier, beatLength, currentTimingPoint, &object, key, true));
            actions.push_back(Action(sliderMultiplier, beatLength, currentTimingPoint, &object, key, false));
        }

        if (lastTime - object.getTime() <= 70 || object.getType() == HitObjectType::Slider)
        {
            if (key == PRIMARY_KEY)
            {
                key = SECONDARY_KEY;
            }
            else
            {
                key = PRIMARY_KEY;
            }
        }
        else
        {
            key = PRIMARY_KEY;
        }

        lastTime = object.getTime();
    }

    beatmap->setActions(actions);

    return *beatmap;
}
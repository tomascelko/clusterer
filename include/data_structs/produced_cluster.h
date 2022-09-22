#pragma once
template<typename hit_type>
class produced_cluster
{
    private:
    cluster<hit_type> cluster_;
    uint32_t producer_id_;
    public:
    uint32_t producer_id()
    {
        return producer_id_;
    }
    cluster<hit_type>& cl()
    {
        return cluster_;
    }
    produced_cluster(cluster<hit_type> && cl, uint32_t id) :
    cluster_(cl),
    producer_id_(id)
    {
    }
    produced_cluster(){}
    static produced_cluster end_token(uint32_t id = 0)
    {
        return produced_cluster(cluster<hit_type>::end_token(), id);
    }
    std::vector<hit_type>& hits()
    {
        return cluster_.hits();
    }
    const std::vector<hit_type>& hits() const
    {
        return cluster_.hits();
    }
    double first_toa() const
    {
        return cluster_.first_toa();
    }
    double last_toa() const
    {
        return cluster_.last_toa();
    }
    bool is_valid()
    {
        return cluster_.is_valid();
    }
    void add_hit(hit_type && hit)
    {
        cluster_.add_hit(std::move(hit));
    }
    void set_first_toa(double toa)
    {
        cluster_.set_first_toa(toa);
    }
    void set_last_toa(double toa)
    {
        cluster_.set_last_toa(toa);
    }
    void set_producer_id(uint16_t id)
    {
        producer_id_ = id;
    }

};